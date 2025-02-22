// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_AFFILIATED_INVALIDATION_SERVICE_PROVIDER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_AFFILIATED_INVALIDATION_SERVICE_PROVIDER_IMPL_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/policy/affiliated_invalidation_service_provider.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace invalidation {
class InvalidationService;
class TiclInvalidationService;
}

namespace policy {

class AffiliatedInvalidationServiceProviderImpl
    : public AffiliatedInvalidationServiceProvider,
      public content::NotificationObserver {
 public:
  AffiliatedInvalidationServiceProviderImpl();
  ~AffiliatedInvalidationServiceProviderImpl() override;

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // AffiliatedInvalidationServiceProvider:
  void RegisterConsumer(Consumer* consumer) override;
  void UnregisterConsumer(Consumer* consumer) override;
  void Shutdown() override;

  invalidation::TiclInvalidationService*
      GetDeviceInvalidationServiceForTest() const;

 private:
  // Helper that monitors the status of a single |InvalidationService|.
  class InvalidationServiceObserver;

  // Status updates received from |InvalidationServiceObserver|s.
  void OnInvalidationServiceConnected(
      invalidation::InvalidationService* invalidation_service);
  void OnInvalidationServiceDisconnected(
      invalidation::InvalidationService* invalidation_service);

  // Checks whether a connected |InvalidationService| affiliated with the
  // device's enrollment domain is available. If so, notifies the consumers.
  // Otherwise, consumers will be notified once such an invalidation service
  // becomes available.
  // Further ensures that a device-global invalidation service is running iff
  // there is no other connected service available for use and there is at least
  // one registered consumer.
  void FindConnectedInvalidationService();

  // Choose |invalidation_service| as the shared invalidation service and notify
  // consumers.
  void SetInvalidationService(
      invalidation::InvalidationService* invalidation_service);

  // Destroy the device-global invalidation service, if any.
  void DestroyDeviceInvalidationService();

  content::NotificationRegistrar registrar_;

  // Device-global invalidation service.
  std::unique_ptr<invalidation::TiclInvalidationService>
      device_invalidation_service_;

  // State observer for the device-global invalidation service.
  std::unique_ptr<InvalidationServiceObserver>
      device_invalidation_service_observer_;

  // State observers for logged-in users' invalidation services.
  std::vector<std::unique_ptr<InvalidationServiceObserver>>
      profile_invalidation_service_observers_;

  // The invalidation service currently used by consumers. nullptr if there are
  // no registered consumers or no connected invalidation service is available
  // for use.
  invalidation::InvalidationService* invalidation_service_;

  base::ObserverList<Consumer, true>::Unchecked consumers_;
  int consumer_count_;

  bool is_shut_down_;

  DISALLOW_COPY_AND_ASSIGN(AffiliatedInvalidationServiceProviderImpl);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_AFFILIATED_INVALIDATION_SERVICE_PROVIDER_IMPL_H_
