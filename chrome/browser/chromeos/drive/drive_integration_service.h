// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_INTEGRATION_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_INTEGRATION_SERVICE_H_

#include <memory>

#include "base/callback.h"
#include "base/feature_list.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chromeos/components/drivefs/drivefs_host.h"
#include "components/drive/drive_notification_observer.h"
#include "components/drive/file_errors.h"
#include "components/drive/file_system_core_util.h"
#include "components/drive/job_scheduler.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Profile;

namespace base {
class FilePath;
class SequencedTaskRunner;
}

namespace drivefs {
class DriveFsHost;

namespace mojom {
class DriveFs;
}  // namespace mojom
}  // namespace drivefs

namespace drive {

class DebugInfoCollector;
class DownloadHandler;
class DriveAppRegistry;
class DriveServiceInterface;
class EventLogger;
class FileSystemInterface;
class JobListInterface;

namespace internal {
class FileCache;
class ResourceMetadata;
class ResourceMetadataStorage;
}  // namespace internal

// Interface for classes that need to observe events from
// DriveIntegrationService.  All events are notified on UI thread.
class DriveIntegrationServiceObserver {
 public:
  // Triggered when the file system is mounted.
  virtual void OnFileSystemMounted() {
  }

  // Triggered when the file system is being unmounted.
  virtual void OnFileSystemBeingUnmounted() {
  }

 protected:
  virtual ~DriveIntegrationServiceObserver() {}
};

// DriveIntegrationService is used to integrate Drive to Chrome. This class
// exposes the file system representation built on top of Drive and some
// other Drive related objects to the file manager, and some other sub
// systems.
//
// The class is essentially a container that manages lifetime of the objects
// that are used to integrate Drive to Chrome. The object of this class is
// created per-profile.
class DriveIntegrationService : public KeyedService,
                                public DriveNotificationObserver,
                                public content::NotificationObserver {
 public:
  class PreferenceWatcher;
  using DriveFsMojoConnectionDelegateFactory = base::RepeatingCallback<
      std::unique_ptr<drivefs::DriveFsHost::MojoConnectionDelegate>()>;

  // test_drive_service, test_mount_point_name, test_cache_root and
  // test_file_system are used by tests to inject customized instances.
  // Pass NULL or the empty value when not interested.
  // |preference_watcher| observes the drive enable preference, and sets the
  // enable state when changed. It can be NULL. The ownership is taken by
  // the DriveIntegrationService.
  DriveIntegrationService(
      Profile* profile,
      PreferenceWatcher* preference_watcher,
      DriveServiceInterface* test_drive_service,
      const std::string& test_mount_point_name,
      const base::FilePath& test_cache_root,
      FileSystemInterface* test_file_system,
      DriveFsMojoConnectionDelegateFactory
          test_drivefs_mojo_connection_delegate_factory = {});
  ~DriveIntegrationService() override;

  // KeyedService override:
  void Shutdown() override;

  void SetEnabled(bool enabled);
  bool is_enabled() const { return enabled_; }

  bool IsMounted() const;

  // Returns the path of the mount point for drive. It is only valid to call if
  // |IsMounted()|.
  base::FilePath GetMountPointPath() const;

  // Returns the path of DriveFS log if enabled or empty path.
  base::FilePath GetDriveFsLogPath() const;

  // Returns true if |local_path| resides inside |GetMountPointPath()|.
  // In this case |drive_path| will contain 'drive' path of this file, e.g.
  // reparented to the mount point.
  // It is only valid to call if |IsMounted()|.
  bool GetRelativeDrivePath(const base::FilePath& local_path,
                            base::FilePath* drive_path) const;

  // Adds and removes the observer.
  void AddObserver(DriveIntegrationServiceObserver* observer);
  void RemoveObserver(DriveIntegrationServiceObserver* observer);

  // DriveNotificationObserver implementation.
  void OnNotificationReceived() override;
  void OnPushNotificationEnabled(bool enabled) override;

  EventLogger* event_logger() { return logger_.get(); }
  DriveServiceInterface* drive_service() { return drive_service_.get(); }
  DebugInfoCollector* debug_info_collector() {
    return debug_info_collector_.get();
  }
  FileSystemInterface* file_system() { return file_system_.get(); }
  DownloadHandler* download_handler() { return download_handler_.get(); }
  DriveAppRegistry* drive_app_registry() { return drive_app_registry_.get(); }
  JobListInterface* job_list() { return scheduler_.get(); }

  // Clears all the local cache file, the local resource metadata, and
  // in-memory Drive app registry, and remounts the file system. |callback|
  // is called with true when this operation is done successfully. Otherwise,
  // |callback| is called with false. |callback| must not be null.
  void ClearCacheAndRemountFileSystem(
      const base::Callback<void(bool)>& callback);

  // Returns the DriveFsHost if it is enabled.
  drivefs::DriveFsHost* GetDriveFsHost() const;

  // Returns the mojo interface to the DriveFs daemon if it is enabled and
  // connected.
  drivefs::mojom::DriveFs* GetDriveFsInterface() const;

 private:
  enum State {
    NOT_INITIALIZED,
    INITIALIZING,
    INITIALIZED,
    REMOUNTING,
  };
  class DriveFsHolder;

  // Returns true if Drive is enabled.
  // Must be called on UI thread.
  bool IsDriveEnabled();

  // Registers remote file system for drive mount point. If DriveFS is enabled,
  // but not yet mounted, this will start it mounting and wait for it to
  // complete before adding the mount point.
  void AddDriveMountPoint();

  // Registers remote file system for drive mount point.
  void AddDriveMountPointAfterMounted();

  // Unregisters drive mount point from File API.
  void RemoveDriveMountPoint();

  // Adds back the drive mount point.
  // Used to implement ClearCacheAndRemountFileSystem().
  void AddBackDriveMountPoint(const base::Callback<void(bool)>& callback,
                              FileError error);

  // Unregisters drive mount point, and if |remount_delay| is specified
  // then tries to add it back after that delay.
  void MaybeRemountFileSystem(base::Optional<base::TimeDelta> remount_delay);

  // Initializes the object. This function should be called before any
  // other functions.
  void Initialize();

  // Called when metadata initialization is done. Continues initialization if
  // the metadata initialization is successful.
  void InitializeAfterMetadataInitialized(FileError error);

  // Change the download directory to the local "Downloads" if the download
  // destination is set under Drive. This must be called when disabling Drive.
  void AvoidDriveAsDownloadDirecotryPreference();

  // content::NotificationObserver overrides.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  friend class DriveIntegrationServiceFactory;

  Profile* profile_;
  State state_;
  bool enabled_;
  // Custom mount point name that can be injected for testing in constructor.
  std::string mount_point_name_;

  base::FilePath cache_root_directory_;
  std::unique_ptr<EventLogger> logger_;
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  std::unique_ptr<internal::ResourceMetadataStorage, util::DestroyHelper>
      metadata_storage_;
  std::unique_ptr<internal::FileCache, util::DestroyHelper> cache_;
  std::unique_ptr<DriveServiceInterface> drive_service_;
  std::unique_ptr<JobScheduler> scheduler_;
  std::unique_ptr<DriveAppRegistry> drive_app_registry_;
  std::unique_ptr<internal::ResourceMetadata, util::DestroyHelper>
      resource_metadata_;
  std::unique_ptr<FileSystemInterface> file_system_;
  std::unique_ptr<DownloadHandler> download_handler_;
  std::unique_ptr<DebugInfoCollector> debug_info_collector_;

  base::ObserverList<DriveIntegrationServiceObserver>::Unchecked observers_;
  std::unique_ptr<PreferenceWatcher> preference_watcher_;
  std::unique_ptr<content::NotificationRegistrar>
      profile_notification_registrar_;

  std::unique_ptr<DriveFsHolder> drivefs_holder_;
  int drivefs_total_failures_count_ = 0;
  int drivefs_consecutive_failures_count_ = 0;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<DriveIntegrationService> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(DriveIntegrationService);
};

// Singleton that owns all instances of DriveIntegrationService and
// associates them with Profiles.
class DriveIntegrationServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  // Factory function used by tests.
  typedef base::Callback<DriveIntegrationService*(Profile* profile)>
      FactoryCallback;

  // Sets and resets a factory function for tests. See below for why we can't
  // use BrowserContextKeyedServiceFactory::SetTestingFactory().
  class ScopedFactoryForTest {
   public:
    explicit ScopedFactoryForTest(FactoryCallback* factory_for_test);
    ~ScopedFactoryForTest();
  };

  // Returns the DriveIntegrationService for |profile|, creating it if it is
  // not yet created.
  static DriveIntegrationService* GetForProfile(Profile* profile);

  // Returns the DriveIntegrationService that is already associated with
  // |profile|, if it is not yet created it will return NULL.
  static DriveIntegrationService* FindForProfile(Profile* profile);

  // Returns the DriveIntegrationServiceFactory instance.
  static DriveIntegrationServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<DriveIntegrationServiceFactory>;

  DriveIntegrationServiceFactory();
  ~DriveIntegrationServiceFactory() override;

  // BrowserContextKeyedServiceFactory overrides.
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  // This is static so it can be set without instantiating the factory. This
  // allows factory creation to be delayed until it normally happens (on profile
  // creation) rather than when tests are set up. DriveIntegrationServiceFactory
  // transitively depends on ExtensionSystemFactory which crashes if created too
  // soon (i.e. before the BrowserProcess exists).
  static FactoryCallback* factory_for_test_;
};

}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_INTEGRATION_SERVICE_H_
