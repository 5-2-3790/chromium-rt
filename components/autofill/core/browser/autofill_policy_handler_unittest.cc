// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/values.h"
#include "components/autofill/core/browser/autofill_policy_handler.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

// Test cases for the Autofill policy setting.
class AutofillPolicyHandlerTest : public testing::Test {};

TEST_F(AutofillPolicyHandlerTest, Default) {
  policy::PolicyMap policy;
  PrefValueMap prefs;
  AutofillPolicyHandler handler;
  handler.ApplyPolicySettings(policy, &prefs);
  EXPECT_FALSE(
      prefs.GetValue(autofill::prefs::kAutofillEnabledDeprecated, nullptr));
}

TEST_F(AutofillPolicyHandlerTest, Enabled) {
  policy::PolicyMap policy;
  policy.Set(policy::key::kAutoFillEnabled, policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
             std::make_unique<base::Value>(true), nullptr);
  PrefValueMap prefs;
  AutofillPolicyHandler handler;
  handler.ApplyPolicySettings(policy, &prefs);

  // Enabling Autofill should not set the pref. Profile and credit card Autofill
  // prefs should also not get set.
  EXPECT_FALSE(
      prefs.GetValue(autofill::prefs::kAutofillEnabledDeprecated, nullptr));
  EXPECT_FALSE(
      prefs.GetValue(autofill::prefs::kAutofillProfileEnabled, nullptr));
  EXPECT_FALSE(
      prefs.GetValue(autofill::prefs::kAutofillCreditCardEnabled, nullptr));
}

TEST_F(AutofillPolicyHandlerTest, Disabled) {
  policy::PolicyMap policy;
  policy.Set(policy::key::kAutoFillEnabled, policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
             std::make_unique<base::Value>(false), nullptr);
  PrefValueMap prefs;
  AutofillPolicyHandler handler;
  handler.ApplyPolicySettings(policy, &prefs);

  // Disabling Autofill by policy should set the pref.
  const base::Value* value = nullptr;
  EXPECT_TRUE(
      prefs.GetValue(autofill::prefs::kAutofillEnabledDeprecated, &value));
  ASSERT_TRUE(value);
  EXPECT_FALSE(value->GetBool());

  // Disabling Autofill by policy should set the profile Autofill pref.
  value = nullptr;
  EXPECT_TRUE(prefs.GetValue(autofill::prefs::kAutofillProfileEnabled, &value));
  ASSERT_TRUE(value);
  EXPECT_FALSE(value->GetBool());

  // Disabling Autofill by policy should set the credit card Autofill pref.
  value = nullptr;
  EXPECT_TRUE(
      prefs.GetValue(autofill::prefs::kAutofillCreditCardEnabled, &value));
  ASSERT_TRUE(value);
  EXPECT_FALSE(value->GetBool());
}

}  // namespace autofill
