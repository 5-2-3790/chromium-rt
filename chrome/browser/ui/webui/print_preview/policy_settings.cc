// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/policy_settings.h"

#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"

namespace printing {

// static
void PolicySettings::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(prefs::kPrintHeaderFooter,
                                HeaderFooterEnforcement::kNotEnforced);
}

}  // namespace printing
