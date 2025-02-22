// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/pending_bookmark_app_manager.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/test/bind_test_util.h"
#include "base/timer/mock_timer.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/web_applications/components/pending_app_manager.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_installation_task.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/crx_file/id_util.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

const char kFooWebAppUrl[] = "https://foo.example";
const char kBarWebAppUrl[] = "https://bar.example";
const char kQuxWebAppUrl[] = "https://qux.example";
const char kXyzWebAppUrl[] = "https://xyz.example";

const char kWrongUrl[] = "https://foobar.example";

web_app::PendingAppManager::AppInfo GetFooAppInfo() {
  return web_app::PendingAppManager::AppInfo::Create(
      GURL(kFooWebAppUrl), web_app::PendingAppManager::LaunchContainer::kTab);
}

web_app::PendingAppManager::AppInfo GetBarAppInfo() {
  return web_app::PendingAppManager::AppInfo::Create(
      GURL(kBarWebAppUrl),
      web_app::PendingAppManager::LaunchContainer::kWindow);
}

web_app::PendingAppManager::AppInfo GetQuxAppInfo() {
  return web_app::PendingAppManager::AppInfo::Create(
      GURL(kQuxWebAppUrl),
      web_app::PendingAppManager::LaunchContainer::kWindow);
}

web_app::PendingAppManager::AppInfo GetXyzAppInfo() {
  return web_app::PendingAppManager::AppInfo::Create(
      GURL(kXyzWebAppUrl),
      web_app::PendingAppManager::LaunchContainer::kWindow);
}

scoped_refptr<Extension> CreateDummyExtension(const std::string& id) {
  return ExtensionBuilder("Dummy name")
      .SetLocation(Manifest::INTERNAL)
      .SetID(id)
      .Build();
}

}  // namespace

class TestBookmarkAppInstallationTask : public BookmarkAppInstallationTask {
 public:
  TestBookmarkAppInstallationTask(Profile* profile,
                                  web_app::PendingAppManager::AppInfo app_info,
                                  bool succeeds)
      : BookmarkAppInstallationTask(profile, std::move(app_info)),
        profile_(profile),
        succeeds_(succeeds) {}
  ~TestBookmarkAppInstallationTask() override = default;

  void InstallWebAppOrShortcutFromWebContents(
      content::WebContents* web_contents,
      BookmarkAppInstallationTask::ResultCallback callback) override {
    BookmarkAppInstallationTask::ResultCode result_code =
        BookmarkAppInstallationTask::ResultCode::kInstallationFailed;
    std::string app_id;
    if (succeeds_) {
      result_code = BookmarkAppInstallationTask::ResultCode::kSuccess;
      app_id = crx_file::id_util::GenerateId("fake_app_id_for:" +
                                             app_info().url.spec());
      ExtensionRegistry* registry = ExtensionRegistry::Get(profile_);
      registry->AddEnabled(CreateDummyExtension(app_id));
    }

    std::move(on_install_called_).Run();
    std::move(callback).Run(
        BookmarkAppInstallationTask::Result(result_code, app_id));
  }

  void SetOnInstallCalled(base::OnceClosure on_install_called) {
    on_install_called_ = std::move(on_install_called);
  }

 private:
  Profile* profile_;
  bool succeeds_;

  base::OnceClosure on_install_called_;

  DISALLOW_COPY_AND_ASSIGN(TestBookmarkAppInstallationTask);
};

class PendingBookmarkAppManagerTest : public ChromeRenderViewHostTestHarness {
 public:
  PendingBookmarkAppManagerTest()
      : test_web_contents_creator_(base::BindRepeating(
            &PendingBookmarkAppManagerTest::CreateTestWebContents,
            base::Unretained(this))),
        successful_installation_task_creator_(base::BindRepeating(
            &PendingBookmarkAppManagerTest::CreateSuccessfulInstallationTask,
            base::Unretained(this))),
        failing_installation_task_creator_(base::BindRepeating(
            &PendingBookmarkAppManagerTest::CreateFailingInstallationTask,
            base::Unretained(this))) {}

  ~PendingBookmarkAppManagerTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    // CrxInstaller in BookmarkAppInstaller needs an ExtensionService, so
    // create one for the profile.
    TestExtensionSystem* test_system =
        static_cast<TestExtensionSystem*>(ExtensionSystem::Get(profile()));
    test_system->CreateExtensionService(base::CommandLine::ForCurrentProcess(),
                                        profile()->GetPath(),
                                        false /* autoupdate_enabled */);
  }

  std::unique_ptr<content::WebContents> CreateTestWebContents(
      Profile* profile) {
    auto web_contents =
        content::WebContentsTester::CreateTestWebContents(profile, nullptr);
    web_contents_tester_ = content::WebContentsTester::For(web_contents.get());
    return web_contents;
  }

  std::unique_ptr<BookmarkAppInstallationTask> CreateInstallationTask(
      Profile* profile,
      web_app::PendingAppManager::AppInfo app_info,
      bool succeeds) {
    auto task = std::make_unique<TestBookmarkAppInstallationTask>(
        profile, std::move(app_info), succeeds);
    auto* task_ptr = task.get();
    task->SetOnInstallCalled(base::BindLambdaForTesting([task_ptr, this]() {
      ++installation_task_run_count_;
      last_app_info_ = task_ptr->app_info().Clone();
    }));
    return task;
  }

  std::unique_ptr<BookmarkAppInstallationTask> CreateSuccessfulInstallationTask(
      Profile* profile,
      web_app::PendingAppManager::AppInfo app_info) {
    return CreateInstallationTask(profile, std::move(app_info),
                                  true /* succeeds */);
  }

  std::unique_ptr<BookmarkAppInstallationTask> CreateFailingInstallationTask(
      Profile* profile,
      web_app::PendingAppManager::AppInfo app_info) {
    return CreateInstallationTask(profile, std::move(app_info),
                                  false /* succeeds */);
  }

  void InstallCallback(const GURL& url,
                       const base::Optional<std::string>& app_id) {
    install_callback_url_ = url;
    last_app_id_ = app_id;
  }

 protected:
  void ResetResults() {
    install_callback_url_.reset();
    last_app_id_.reset();
    installation_task_run_count_ = 0;
  }

  const PendingBookmarkAppManager::WebContentsFactory&
  test_web_contents_creator() {
    return test_web_contents_creator_;
  }

  const PendingBookmarkAppManager::TaskFactory&
  successful_installation_task_creator() {
    return successful_installation_task_creator_;
  }

  const PendingBookmarkAppManager::TaskFactory&
  failing_installation_task_creator() {
    return failing_installation_task_creator_;
  }

  std::unique_ptr<PendingBookmarkAppManager>
  GetPendingBookmarkAppManagerWithTestFactories() {
    auto manager = std::make_unique<PendingBookmarkAppManager>(profile());
    manager->SetFactoriesForTesting(test_web_contents_creator(),
                                    successful_installation_task_creator());
    return manager;
  }

  void SuccessfullyLoad(const GURL& url) {
    web_contents_tester_->NavigateAndCommit(url);
    web_contents_tester_->TestDidFinishLoad(url);
  }

  content::WebContentsTester* web_contents_tester() {
    return web_contents_tester_;
  }

  bool install_succeeded() { return last_app_id_.has_value(); }

  const GURL& install_callback_url() { return install_callback_url_.value(); }

  const std::string& last_app_id() { return last_app_id_.value(); }

  const web_app::PendingAppManager::AppInfo& last_app_info() {
    CHECK(last_app_info_.get());
    return *last_app_info_;
  }

  // Number of times BookmarkAppInstallationTask::InstallWebAppOrShorcut was
  // called. Reflects how many times we've tried to create an Extension.
  size_t installation_task_run_count() { return installation_task_run_count_; }

 private:
  content::WebContentsTester* web_contents_tester_ = nullptr;
  base::Optional<GURL> install_callback_url_;
  base::Optional<std::string> last_app_id_;
  std::unique_ptr<web_app::PendingAppManager::AppInfo> last_app_info_;
  size_t installation_task_run_count_ = 0;

  PendingBookmarkAppManager::WebContentsFactory test_web_contents_creator_;
  PendingBookmarkAppManager::TaskFactory successful_installation_task_creator_;
  PendingBookmarkAppManager::TaskFactory failing_installation_task_creator_;

  DISALLOW_COPY_AND_ASSIGN(PendingBookmarkAppManagerTest);
};

TEST_F(PendingBookmarkAppManagerTest, Install_Succeeds) {
  auto pending_app_manager = GetPendingBookmarkAppManagerWithTestFactories();
  pending_app_manager->Install(
      GetFooAppInfo(),
      base::BindOnce(&PendingBookmarkAppManagerTest::InstallCallback,
                     base::Unretained(this)));

  base::RunLoop().RunUntilIdle();
  SuccessfullyLoad(GURL(kFooWebAppUrl));

  EXPECT_EQ(1u, installation_task_run_count());
  EXPECT_TRUE(install_succeeded());
  EXPECT_EQ(GetFooAppInfo(), last_app_info());
  EXPECT_EQ(GURL(kFooWebAppUrl), install_callback_url());
}

TEST_F(PendingBookmarkAppManagerTest, Install_SerialCallsDifferentApps) {
  auto pending_app_manager = GetPendingBookmarkAppManagerWithTestFactories();
  pending_app_manager->Install(
      GetFooAppInfo(),
      base::BindOnce(&PendingBookmarkAppManagerTest::InstallCallback,
                     base::Unretained(this)));

  base::RunLoop().RunUntilIdle();
  SuccessfullyLoad(GURL(kFooWebAppUrl));

  EXPECT_EQ(1u, installation_task_run_count());
  EXPECT_TRUE(install_succeeded());
  EXPECT_EQ(GetFooAppInfo(), last_app_info());
  EXPECT_EQ(GURL(kFooWebAppUrl), install_callback_url());
  ResetResults();

  pending_app_manager->Install(
      GetBarAppInfo(),
      base::BindOnce(&PendingBookmarkAppManagerTest::InstallCallback,
                     base::Unretained(this)));

  base::RunLoop().RunUntilIdle();
  SuccessfullyLoad(GURL(kBarWebAppUrl));

  EXPECT_EQ(1u, installation_task_run_count());
  EXPECT_TRUE(install_succeeded());
  EXPECT_EQ(GetBarAppInfo(), last_app_info());
  EXPECT_EQ(GURL(kBarWebAppUrl), install_callback_url());
}

TEST_F(PendingBookmarkAppManagerTest, Install_ConcurrentCallsDifferentApps) {
  auto pending_app_manager = GetPendingBookmarkAppManagerWithTestFactories();
  pending_app_manager->Install(
      GetFooAppInfo(),
      base::BindOnce(&PendingBookmarkAppManagerTest::InstallCallback,
                     base::Unretained(this)));
  pending_app_manager->Install(
      GetBarAppInfo(),
      base::BindOnce(&PendingBookmarkAppManagerTest::InstallCallback,
                     base::Unretained(this)));

  // The last call to Install gets higher priority.
  base::RunLoop().RunUntilIdle();
  SuccessfullyLoad(GURL(kBarWebAppUrl));

  EXPECT_EQ(1u, installation_task_run_count());
  EXPECT_TRUE(install_succeeded());
  EXPECT_EQ(GetBarAppInfo(), last_app_info());
  EXPECT_EQ(GURL(kBarWebAppUrl), install_callback_url());
  ResetResults();

  // Then the first call to Install gets processed.
  base::RunLoop().RunUntilIdle();
  SuccessfullyLoad(GURL(kFooWebAppUrl));

  EXPECT_EQ(1u, installation_task_run_count());
  EXPECT_TRUE(install_succeeded());
  EXPECT_EQ(GetFooAppInfo(), last_app_info());
  EXPECT_EQ(GURL(kFooWebAppUrl), install_callback_url());
}

TEST_F(PendingBookmarkAppManagerTest, Install_PendingSuccessfulTask) {
  auto pending_app_manager = GetPendingBookmarkAppManagerWithTestFactories();
  pending_app_manager->Install(
      GetFooAppInfo(),
      base::BindOnce(&PendingBookmarkAppManagerTest::InstallCallback,
                     base::Unretained(this)));
  // Make sure the installation has started.
  base::RunLoop().RunUntilIdle();

  pending_app_manager->Install(
      GetBarAppInfo(),
      base::BindOnce(&PendingBookmarkAppManagerTest::InstallCallback,
                     base::Unretained(this)));

  // Finish the first install.
  base::RunLoop().RunUntilIdle();
  SuccessfullyLoad(GURL(kFooWebAppUrl));

  EXPECT_EQ(1u, installation_task_run_count());
  EXPECT_TRUE(install_succeeded());
  EXPECT_EQ(GetFooAppInfo(), last_app_info());
  EXPECT_EQ(GURL(kFooWebAppUrl), install_callback_url());
  ResetResults();

  // Finish the second install.
  base::RunLoop().RunUntilIdle();
  SuccessfullyLoad(GURL(kBarWebAppUrl));

  EXPECT_EQ(1u, installation_task_run_count());
  EXPECT_TRUE(install_succeeded());
  EXPECT_EQ(GetBarAppInfo(), last_app_info());
  EXPECT_EQ(GURL(kBarWebAppUrl), install_callback_url());
}

TEST_F(PendingBookmarkAppManagerTest, Install_PendingFailingTask) {
  auto pending_app_manager = GetPendingBookmarkAppManagerWithTestFactories();
  pending_app_manager->Install(
      GetFooAppInfo(),
      base::BindOnce(&PendingBookmarkAppManagerTest::InstallCallback,
                     base::Unretained(this)));
  // Make sure the installation has started.
  base::RunLoop().RunUntilIdle();

  pending_app_manager->Install(
      GetBarAppInfo(),
      base::BindOnce(&PendingBookmarkAppManagerTest::InstallCallback,
                     base::Unretained(this)));

  // Fail the first install.
  base::RunLoop().RunUntilIdle();
  SuccessfullyLoad(GURL(kWrongUrl));

  // The installation didn't run because we loaded the wrong url.
  EXPECT_EQ(0u, installation_task_run_count());
  EXPECT_FALSE(install_succeeded());
  EXPECT_EQ(GURL(kFooWebAppUrl), install_callback_url());
  ResetResults();

  // Finish the second install.
  base::RunLoop().RunUntilIdle();
  SuccessfullyLoad(GURL(kBarWebAppUrl));

  EXPECT_EQ(1u, installation_task_run_count());
  EXPECT_TRUE(install_succeeded());
  EXPECT_EQ(GetBarAppInfo(), last_app_info());
  EXPECT_EQ(GURL(kBarWebAppUrl), install_callback_url());
}

TEST_F(PendingBookmarkAppManagerTest, Install_ReentrantCallback) {
  auto pending_app_manager = GetPendingBookmarkAppManagerWithTestFactories();
  // Call install with a callback that tries to install another app.
  pending_app_manager->Install(
      GetFooAppInfo(),
      base::BindLambdaForTesting(
          [&](const GURL& provided_url,
              const base::Optional<std::string>& app_id) {
            InstallCallback(provided_url, app_id);
            pending_app_manager->Install(
                GetBarAppInfo(),
                base::BindOnce(&PendingBookmarkAppManagerTest::InstallCallback,
                               base::Unretained(this)));
          }));

  // Finish the first install.
  base::RunLoop().RunUntilIdle();
  SuccessfullyLoad(GURL(kFooWebAppUrl));

  EXPECT_EQ(1u, installation_task_run_count());
  EXPECT_TRUE(install_succeeded());
  EXPECT_EQ(GetFooAppInfo(), last_app_info());
  EXPECT_EQ(GURL(kFooWebAppUrl), install_callback_url());
  ResetResults();

  base::RunLoop().RunUntilIdle();
  SuccessfullyLoad(GURL(kBarWebAppUrl));

  EXPECT_EQ(1u, installation_task_run_count());
  EXPECT_TRUE(install_succeeded());
  EXPECT_EQ(GetBarAppInfo(), last_app_info());
  EXPECT_EQ(GURL(kBarWebAppUrl), install_callback_url());
}

TEST_F(PendingBookmarkAppManagerTest, Install_SerialCallsSameApp) {
  auto pending_app_manager = GetPendingBookmarkAppManagerWithTestFactories();
  pending_app_manager->Install(
      GetFooAppInfo(),
      base::BindOnce(&PendingBookmarkAppManagerTest::InstallCallback,
                     base::Unretained(this)));

  base::RunLoop().RunUntilIdle();
  SuccessfullyLoad(GURL(kFooWebAppUrl));

  EXPECT_EQ(1u, installation_task_run_count());
  EXPECT_TRUE(install_succeeded());
  EXPECT_EQ(GURL(kFooWebAppUrl), install_callback_url());
  ResetResults();

  pending_app_manager->Install(
      GetFooAppInfo(),
      base::BindOnce(&PendingBookmarkAppManagerTest::InstallCallback,
                     base::Unretained(this)));

  base::RunLoop().RunUntilIdle();

  // The app is already installed so we shouldn't try to install it again.
  EXPECT_EQ(0u, installation_task_run_count());
  EXPECT_TRUE(install_succeeded());
  EXPECT_EQ(GURL(kFooWebAppUrl), install_callback_url());
}

TEST_F(PendingBookmarkAppManagerTest, Install_ConcurrentCallsSameApp) {
  auto pending_app_manager = GetPendingBookmarkAppManagerWithTestFactories();
  pending_app_manager->Install(
      GetFooAppInfo(),
      base::BindOnce(&PendingBookmarkAppManagerTest::InstallCallback,
                     base::Unretained(this)));
  pending_app_manager->Install(
      GetFooAppInfo(),
      base::BindOnce(&PendingBookmarkAppManagerTest::InstallCallback,
                     base::Unretained(this)));

  base::RunLoop().RunUntilIdle();
  SuccessfullyLoad(GURL(kFooWebAppUrl));

  EXPECT_EQ(1u, installation_task_run_count());
  EXPECT_TRUE(install_succeeded());
  EXPECT_EQ(GetFooAppInfo(), last_app_info());
  EXPECT_EQ(GURL(kFooWebAppUrl), install_callback_url());
  ResetResults();

  base::RunLoop().RunUntilIdle();

  // The second installation should succeed even though the app is installed
  // already.
  EXPECT_EQ(0u, installation_task_run_count());
  EXPECT_TRUE(install_succeeded());
  EXPECT_EQ(GURL(kFooWebAppUrl), install_callback_url());
}

TEST_F(PendingBookmarkAppManagerTest, Install_FailsLoadIncorrectURL) {
  auto pending_app_manager = GetPendingBookmarkAppManagerWithTestFactories();
  pending_app_manager->Install(
      GetFooAppInfo(),
      base::BindOnce(&PendingBookmarkAppManagerTest::InstallCallback,
                     base::Unretained(this)));

  base::RunLoop().RunUntilIdle();
  SuccessfullyLoad(GURL(kWrongUrl));

  EXPECT_EQ(0u, installation_task_run_count());
  EXPECT_FALSE(install_succeeded());
  EXPECT_EQ(GURL(kFooWebAppUrl), install_callback_url());
}

TEST_F(PendingBookmarkAppManagerTest, InstallApps_Succeeds) {
  auto pending_app_manager = GetPendingBookmarkAppManagerWithTestFactories();
  std::vector<web_app::PendingAppManager::AppInfo> apps_to_install;
  apps_to_install.push_back(GetFooAppInfo());

  pending_app_manager->InstallApps(
      std::move(apps_to_install),
      base::BindRepeating(&PendingBookmarkAppManagerTest::InstallCallback,
                          base::Unretained(this)));

  base::RunLoop().RunUntilIdle();
  SuccessfullyLoad(GURL(kFooWebAppUrl));

  EXPECT_EQ(1u, installation_task_run_count());
  EXPECT_TRUE(install_succeeded());
  EXPECT_EQ(GetFooAppInfo(), last_app_info());
  EXPECT_EQ(GURL(kFooWebAppUrl), install_callback_url());
}

TEST_F(PendingBookmarkAppManagerTest, InstallApps_Fails) {
  auto pending_app_manager = GetPendingBookmarkAppManagerWithTestFactories();
  std::vector<web_app::PendingAppManager::AppInfo> apps_to_install;
  apps_to_install.push_back(GetFooAppInfo());

  pending_app_manager->InstallApps(
      std::move(apps_to_install),
      base::BindRepeating(&PendingBookmarkAppManagerTest::InstallCallback,
                          base::Unretained(this)));

  base::RunLoop().RunUntilIdle();
  SuccessfullyLoad(GURL(kWrongUrl));

  EXPECT_EQ(0u, installation_task_run_count());
  EXPECT_FALSE(install_succeeded());
  EXPECT_EQ(GURL(kFooWebAppUrl), install_callback_url());
}

TEST_F(PendingBookmarkAppManagerTest, InstallApps_Multiple) {
  auto pending_app_manager = GetPendingBookmarkAppManagerWithTestFactories();

  std::vector<web_app::PendingAppManager::AppInfo> apps_to_install;
  apps_to_install.push_back(GetFooAppInfo());
  apps_to_install.push_back(GetBarAppInfo());

  pending_app_manager->InstallApps(
      std::move(apps_to_install),
      base::BindRepeating(&PendingBookmarkAppManagerTest::InstallCallback,
                          base::Unretained(this)));

  // Finish the first install.
  base::RunLoop().RunUntilIdle();
  SuccessfullyLoad(GURL(kFooWebAppUrl));

  EXPECT_EQ(1u, installation_task_run_count());
  EXPECT_TRUE(install_succeeded());
  EXPECT_EQ(GetFooAppInfo(), last_app_info());
  EXPECT_EQ(GURL(kFooWebAppUrl), install_callback_url());
  ResetResults();

  // Finish the second install.
  base::RunLoop().RunUntilIdle();
  SuccessfullyLoad(GURL(kBarWebAppUrl));

  EXPECT_EQ(1u, installation_task_run_count());
  EXPECT_TRUE(install_succeeded());
  EXPECT_EQ(GetBarAppInfo(), last_app_info());
  EXPECT_EQ(GURL(kBarWebAppUrl), install_callback_url());
}

TEST_F(PendingBookmarkAppManagerTest, InstallApps_PendingInstallApps) {
  auto pending_app_manager = GetPendingBookmarkAppManagerWithTestFactories();

  {
    std::vector<web_app::PendingAppManager::AppInfo> apps_to_install;
    apps_to_install.push_back(GetFooAppInfo());

    pending_app_manager->InstallApps(
        std::move(apps_to_install),
        base::BindRepeating(&PendingBookmarkAppManagerTest::InstallCallback,
                            base::Unretained(this)));
  }

  {
    std::vector<web_app::PendingAppManager::AppInfo> apps_to_install;
    apps_to_install.push_back(GetBarAppInfo());

    pending_app_manager->InstallApps(
        std::move(apps_to_install),
        base::BindRepeating(&PendingBookmarkAppManagerTest::InstallCallback,
                            base::Unretained(this)));
  }

  // Finish the first install.
  base::RunLoop().RunUntilIdle();
  SuccessfullyLoad(GURL(kFooWebAppUrl));

  EXPECT_EQ(1u, installation_task_run_count());
  EXPECT_TRUE(install_succeeded());
  EXPECT_EQ(GetFooAppInfo(), last_app_info());
  EXPECT_EQ(GURL(kFooWebAppUrl), install_callback_url());
  ResetResults();

  // Finish the second install.
  base::RunLoop().RunUntilIdle();
  SuccessfullyLoad(GURL(kBarWebAppUrl));

  EXPECT_EQ(1u, installation_task_run_count());
  EXPECT_TRUE(install_succeeded());
  EXPECT_EQ(GetBarAppInfo(), last_app_info());
  EXPECT_EQ(GURL(kBarWebAppUrl), install_callback_url());
}

TEST_F(PendingBookmarkAppManagerTest, Install_PendingMulitpleInstallApps) {
  auto pending_app_manager = GetPendingBookmarkAppManagerWithTestFactories();

  std::vector<web_app::PendingAppManager::AppInfo> apps_to_install;
  apps_to_install.push_back(GetFooAppInfo());
  apps_to_install.push_back(GetBarAppInfo());

  // Queue through InstallApps.
  pending_app_manager->InstallApps(
      std::move(apps_to_install),
      base::BindRepeating(&PendingBookmarkAppManagerTest::InstallCallback,
                          base::Unretained(this)));

  // Queue through Install.
  pending_app_manager->Install(
      GetQuxAppInfo(),
      base::BindOnce(&PendingBookmarkAppManagerTest::InstallCallback,
                     base::Unretained(this)));

  // The install request from Install should be processed first.
  base::RunLoop().RunUntilIdle();
  SuccessfullyLoad(GURL(kQuxWebAppUrl));

  EXPECT_EQ(1u, installation_task_run_count());
  EXPECT_TRUE(install_succeeded());
  EXPECT_EQ(GetQuxAppInfo(), last_app_info());
  EXPECT_EQ(GURL(kQuxWebAppUrl), install_callback_url());
  ResetResults();

  // The install requests from InstallApps should be processed next.
  base::RunLoop().RunUntilIdle();
  SuccessfullyLoad(GURL(kFooWebAppUrl));

  EXPECT_EQ(1u, installation_task_run_count());
  EXPECT_TRUE(install_succeeded());
  EXPECT_EQ(GetFooAppInfo(), last_app_info());
  EXPECT_EQ(GURL(kFooWebAppUrl), install_callback_url());
  ResetResults();

  base::RunLoop().RunUntilIdle();
  SuccessfullyLoad(GURL(kBarWebAppUrl));

  EXPECT_EQ(1u, installation_task_run_count());
  EXPECT_TRUE(install_succeeded());
  EXPECT_EQ(GetBarAppInfo(), last_app_info());
  EXPECT_EQ(GURL(kBarWebAppUrl), install_callback_url());
}

TEST_F(PendingBookmarkAppManagerTest, InstallApps_PendingInstall) {
  auto pending_app_manager = GetPendingBookmarkAppManagerWithTestFactories();
  // Queue through Install.
  pending_app_manager->Install(
      GetQuxAppInfo(),
      base::BindOnce(&PendingBookmarkAppManagerTest::InstallCallback,
                     base::Unretained(this)));

  // Queue through InstallApps.
  std::vector<web_app::PendingAppManager::AppInfo> apps_to_install;
  apps_to_install.push_back(GetFooAppInfo());
  apps_to_install.push_back(GetBarAppInfo());

  pending_app_manager->InstallApps(
      std::move(apps_to_install),
      base::BindRepeating(&PendingBookmarkAppManagerTest::InstallCallback,
                          base::Unretained(this)));

  // The install request from Install should be processed first.
  base::RunLoop().RunUntilIdle();
  SuccessfullyLoad(GURL(kQuxWebAppUrl));

  EXPECT_EQ(1u, installation_task_run_count());
  EXPECT_TRUE(install_succeeded());
  EXPECT_EQ(GetQuxAppInfo(), last_app_info());
  EXPECT_EQ(GURL(kQuxWebAppUrl), install_callback_url());
  ResetResults();

  // The install requests from InstallApps should be processed next.
  base::RunLoop().RunUntilIdle();
  SuccessfullyLoad(GURL(kFooWebAppUrl));

  EXPECT_EQ(1u, installation_task_run_count());
  EXPECT_TRUE(install_succeeded());
  EXPECT_EQ(GURL(kFooWebAppUrl), install_callback_url());
  EXPECT_EQ(GetFooAppInfo(), last_app_info());
  ResetResults();

  base::RunLoop().RunUntilIdle();
  SuccessfullyLoad(GURL(kBarWebAppUrl));

  EXPECT_EQ(1u, installation_task_run_count());
  EXPECT_TRUE(install_succeeded());
  EXPECT_EQ(GetBarAppInfo(), last_app_info());
  EXPECT_EQ(GURL(kBarWebAppUrl), install_callback_url());
}

TEST_F(PendingBookmarkAppManagerTest, WebContentsLoadTimedOut) {
  auto pending_app_manager = GetPendingBookmarkAppManagerWithTestFactories();
  auto timer_to_pass = std::make_unique<base::MockOneShotTimer>();
  auto* timer = timer_to_pass.get();

  pending_app_manager->SetTimerForTesting(std::move(timer_to_pass));

  // Queue an app through Install.
  pending_app_manager->Install(
      GetQuxAppInfo(),
      base::BindOnce(&PendingBookmarkAppManagerTest::InstallCallback,
                     base::Unretained(this)));
  base::RunLoop().RunUntilIdle();

  // Verify that the timer is stopped after a successful load.
  EXPECT_TRUE(timer->IsRunning());
  SuccessfullyLoad(GURL(kQuxWebAppUrl));
  EXPECT_FALSE(timer->IsRunning());
  EXPECT_EQ(1u, installation_task_run_count());
  EXPECT_TRUE(install_succeeded());
  EXPECT_EQ(GURL(kQuxWebAppUrl), install_callback_url());
  ResetResults();

  // Queue a different app through Install.
  pending_app_manager->Install(
      GetXyzAppInfo(),
      base::BindOnce(&PendingBookmarkAppManagerTest::InstallCallback,
                     base::Unretained(this)));
  base::RunLoop().RunUntilIdle();

  // Fire the timer to simulate a failed load.
  EXPECT_TRUE(timer->IsRunning());
  timer->Fire();
  EXPECT_FALSE(install_succeeded());
  EXPECT_EQ(GURL(kXyzWebAppUrl), install_callback_url());
  ResetResults();

  // Queue two more apps, different from all those before, through InstallApps.
  std::vector<web_app::PendingAppManager::AppInfo> apps_to_install;
  apps_to_install.push_back(GetFooAppInfo());
  apps_to_install.push_back(GetBarAppInfo());

  pending_app_manager->InstallApps(
      std::move(apps_to_install),
      base::BindRepeating(&PendingBookmarkAppManagerTest::InstallCallback,
                          base::Unretained(this)));

  base::RunLoop().RunUntilIdle();

  // Fire the timer to simulate a failed load.
  EXPECT_TRUE(timer->IsRunning());
  timer->Fire();
  EXPECT_FALSE(install_succeeded());
  EXPECT_EQ(GURL(kFooWebAppUrl), install_callback_url());
  ResetResults();

  base::RunLoop().RunUntilIdle();

  // Fire the timer to simulate a failed load.
  EXPECT_TRUE(timer->IsRunning());
  timer->Fire();
  EXPECT_FALSE(install_succeeded());
  EXPECT_EQ(GURL(kBarWebAppUrl), install_callback_url());
  ResetResults();

  // Ensure a successful load after a timer fire works.
  pending_app_manager->Install(
      GetBarAppInfo(),
      base::BindOnce(&PendingBookmarkAppManagerTest::InstallCallback,
                     base::Unretained(this)));
  base::RunLoop().RunUntilIdle();

  // Verify that the timer is stopped after a successful load.
  EXPECT_TRUE(timer->IsRunning());
  SuccessfullyLoad(GURL(kBarWebAppUrl));
  EXPECT_FALSE(timer->IsRunning());
  EXPECT_EQ(1u, installation_task_run_count());
  EXPECT_TRUE(install_succeeded());
  EXPECT_EQ(GetBarAppInfo(), last_app_info());
  EXPECT_EQ(GURL(kBarWebAppUrl), install_callback_url());
}

TEST_F(PendingBookmarkAppManagerTest, ExtensionUninstalled) {
  auto pending_app_manager = GetPendingBookmarkAppManagerWithTestFactories();
  pending_app_manager->Install(
      GetFooAppInfo(),
      base::BindOnce(&PendingBookmarkAppManagerTest::InstallCallback,
                     base::Unretained(this)));

  base::RunLoop().RunUntilIdle();
  SuccessfullyLoad(GURL(kFooWebAppUrl));

  EXPECT_EQ(1u, installation_task_run_count());
  EXPECT_TRUE(install_succeeded());

  const std::string app_id = last_app_id();
  ResetResults();

  // Simulate the extension for the app getting uninstalled.
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile());
  registry->RemoveEnabled(app_id);

  pending_app_manager->Install(
      GetFooAppInfo(),
      base::BindOnce(&PendingBookmarkAppManagerTest::InstallCallback,
                     base::Unretained(this)));

  base::RunLoop().RunUntilIdle();
  SuccessfullyLoad(GURL(kFooWebAppUrl));

  // The extension was uninstalled so a new installation task should run.
  EXPECT_EQ(1u, installation_task_run_count());
  EXPECT_TRUE(install_succeeded());
}

TEST_F(PendingBookmarkAppManagerTest, ExternalExtensionUninstalled) {
  auto pending_app_manager = GetPendingBookmarkAppManagerWithTestFactories();
  pending_app_manager->Install(
      GetFooAppInfo(),
      base::BindOnce(&PendingBookmarkAppManagerTest::InstallCallback,
                     base::Unretained(this)));

  base::RunLoop().RunUntilIdle();
  SuccessfullyLoad(GURL(kFooWebAppUrl));

  EXPECT_EQ(1u, installation_task_run_count());
  EXPECT_TRUE(install_succeeded());

  const std::string app_id = last_app_id();
  ResetResults();

  // Simulate external extension for the app getting uninstalled by the user.
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile());
  registry->RemoveEnabled(app_id);
  ExtensionPrefs::Get(profile())->OnExtensionUninstalled(
      app_id, Manifest::EXTERNAL_POLICY, false /* external_uninstall */);

  pending_app_manager->Install(
      GetFooAppInfo(),
      base::BindOnce(&PendingBookmarkAppManagerTest::InstallCallback,
                     base::Unretained(this)));
  base::RunLoop().RunUntilIdle();

  // The extension was uninstalled by the user, we shouldn't try to install it
  // again.
  EXPECT_EQ(0u, installation_task_run_count());
  EXPECT_FALSE(install_succeeded());
}

}  // namespace extensions
