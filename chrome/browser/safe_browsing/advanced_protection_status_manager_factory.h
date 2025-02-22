// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_ADVANCED_PROTECTION_STATUS_MANAGER_FACTORY_H_
#define CHROME_BROWSER_SAFE_BROWSING_ADVANCED_PROTECTION_STATUS_MANAGER_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace safe_browsing {

class AdvancedProtectionStatusManager;

// Responsible for keeping track of advanced protection status of the sign-in
// profile.
class AdvancedProtectionStatusManagerFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static AdvancedProtectionStatusManager* GetForBrowserContext(
      content::BrowserContext* context);

  static AdvancedProtectionStatusManagerFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<
      AdvancedProtectionStatusManagerFactory>;
  AdvancedProtectionStatusManagerFactory();
  ~AdvancedProtectionStatusManagerFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace safe_browsing
#endif  // CHROME_BROWSER_SAFE_BROWSING_ADVANCED_PROTECTION_STATUS_MANAGER_FACTORY_H_
