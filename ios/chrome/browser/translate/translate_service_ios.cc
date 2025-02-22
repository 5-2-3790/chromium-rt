// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/translate/translate_service_ios.h"

#include "base/logging.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_manager.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "url/gurl.h"

namespace {
// The singleton instance of TranslateServiceIOS.
TranslateServiceIOS* g_translate_service = nullptr;
}

TranslateServiceIOS::TranslateServiceIOS()
    : resource_request_allowed_notifier_(
          GetApplicationContext()->GetLocalState(),
          nullptr,
          base::BindOnce(&ApplicationContext::GetNetworkConnectionTracker,
                         base::Unretained(GetApplicationContext()))) {
  resource_request_allowed_notifier_.Init(this, false /* leaky */);
}

TranslateServiceIOS::~TranslateServiceIOS() {
}

// static
void TranslateServiceIOS::Initialize() {
  if (g_translate_service)
    return;

  g_translate_service = new TranslateServiceIOS;
  // Initialize the allowed state for resource requests.
  g_translate_service->OnResourceRequestsAllowed();
  translate::TranslateDownloadManager* download_manager =
      translate::TranslateDownloadManager::GetInstance();
  download_manager->set_url_loader_factory(
      GetApplicationContext()->GetSharedURLLoaderFactory());
  download_manager->set_application_locale(
      GetApplicationContext()->GetApplicationLocale());
}

// static
void TranslateServiceIOS::Shutdown() {
  translate::TranslateDownloadManager* download_manager =
      translate::TranslateDownloadManager::GetInstance();
  download_manager->Shutdown();
}

void TranslateServiceIOS::OnResourceRequestsAllowed() {
  translate::TranslateLanguageList* language_list =
      translate::TranslateDownloadManager::GetInstance()->language_list();
  if (!language_list) {
    NOTREACHED();
    return;
  }

  language_list->SetResourceRequestsAllowed(
      resource_request_allowed_notifier_.ResourceRequestsAllowed());
}

// static
bool TranslateServiceIOS::IsTranslatableURL(const GURL& url) {
  // A URL is translatable unless it is one of the following:
  // - empty (can happen for popups created with window.open(""))
  // - an internal URL
  return !url.is_empty() && !url.SchemeIs(kChromeUIScheme);
}
