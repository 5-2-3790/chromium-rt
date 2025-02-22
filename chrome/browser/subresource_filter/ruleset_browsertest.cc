// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/subresource_filter/subresource_filter_browser_test_harness.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "chrome/browser/browser_process.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/subresource_filter/content/browser/async_document_subresource_filter.h"
#include "components/subresource_filter/content/browser/async_document_subresource_filter_test_utils.h"
#include "components/subresource_filter/content/browser/content_ruleset_service.h"
#include "components/subresource_filter/core/browser/ruleset_service.h"
#include "components/subresource_filter/core/common/common_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace subresource_filter {

namespace {

void OpenAndPublishRuleset(ContentRulesetService* content_ruleset_service,
                           const base::FilePath& path) {
  base::File index_file;
  base::RunLoop open_loop;
  auto open_callback = base::BindRepeating(
      [](base::OnceClosure quit_closure, base::File* out, base::File result) {
        *out = std::move(result);
        std::move(quit_closure).Run();
      },
      open_loop.QuitClosure(), &index_file);
  IndexedRulesetVersion version =
      content_ruleset_service->GetMostRecentlyIndexedVersion();
  content_ruleset_service->TryOpenAndSetRulesetFile(path, version.checksum,
                                                    std::move(open_callback));
  open_loop.Run();
  ASSERT_TRUE(index_file.IsValid());
  content_ruleset_service->PublishNewRulesetVersion(std::move(index_file));
}

RulesetVerificationStatus GetRulesetVerification() {
  ContentRulesetService* service =
      g_browser_process->subresource_filter_ruleset_service();
  VerifiedRulesetDealer::Handle* dealer_handle = service->ruleset_dealer();

  auto callback_method = [](base::OnceClosure quit_closure,
                            RulesetVerificationStatus* status,
                            VerifiedRulesetDealer* verified_dealer) {
    *status = verified_dealer->status();
    std::move(quit_closure).Run();
  };

  RulesetVerificationStatus status;
  base::RunLoop run_loop;
  auto callback =
      base::BindRepeating(callback_method, run_loop.QuitClosure(), &status);

  dealer_handle->GetDealerAsync(callback);
  run_loop.Run();
  return status;
}

}  // namespace

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       RulesetVerified_Activation) {
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  ContentRulesetService* service =
      g_browser_process->subresource_filter_ruleset_service();
  ASSERT_TRUE(service->ruleset_dealer());
  auto ruleset_handle =
      std::make_unique<VerifiedRuleset::Handle>(service->ruleset_dealer());
  AsyncDocumentSubresourceFilter::InitializationParams params(
      GURL("https://example.com/"), ActivationLevel::ENABLED, false);

  testing::TestActivationStateCallbackReceiver receiver;
  AsyncDocumentSubresourceFilter filter(ruleset_handle.get(), std::move(params),
                                        receiver.GetCallback());
  receiver.WaitForActivationDecision();
  receiver.ExpectReceivedOnce(ActivationState(ActivationLevel::ENABLED));
}

// TODO(ericrobinson): Add a test using a PRE_ phase that corrupts the ruleset
// on disk to test something closer to an actual execution path for checksum.

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest, NoRuleset_NoActivation) {
  // Do not set the ruleset, which results in an invalid ruleset.
  ContentRulesetService* service =
      g_browser_process->subresource_filter_ruleset_service();
  ASSERT_TRUE(service->ruleset_dealer());
  auto ruleset_handle =
      std::make_unique<VerifiedRuleset::Handle>(service->ruleset_dealer());
  AsyncDocumentSubresourceFilter::InitializationParams params(
      GURL("https://example.com/"), ActivationLevel::ENABLED, false);

  testing::TestActivationStateCallbackReceiver receiver;
  AsyncDocumentSubresourceFilter filter(ruleset_handle.get(), std::move(params),
                                        receiver.GetCallback());
  receiver.WaitForActivationDecision();
  receiver.ExpectReceivedOnce(ActivationState(ActivationLevel::DISABLED));
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest, InvalidRuleset_Checksum) {
  const char kTestRulesetSuffix[] = "foo";
  const int kNumberOfRules = 500;
  TestRulesetCreator ruleset_creator;
  TestRulesetPair test_ruleset_pair;
  ASSERT_NO_FATAL_FAILURE(
      ruleset_creator.CreateRulesetToDisallowURLsWithManySuffixes(
          kTestRulesetSuffix, kNumberOfRules, &test_ruleset_pair));
  ContentRulesetService* service =
      g_browser_process->subresource_filter_ruleset_service();

  // Publish the good ruleset.
  TestRulesetPublisher publisher;
  publisher.SetRuleset(test_ruleset_pair.unindexed);

  // Now corrupt it by flipping one entry.  This can only be detected
  // via the checksum, and not the Flatbuffer Verifier.  This was determined
  // at random by flipping elements until this test failed, then adding
  // the checksum code and ensuring it passed.
  testing::TestRuleset::CorruptByFilling(test_ruleset_pair.indexed, 28250,
                                         28251, 32);
  OpenAndPublishRuleset(service, test_ruleset_pair.indexed.path);
  ASSERT_TRUE(service->ruleset_dealer());

  auto ruleset_handle =
      std::make_unique<VerifiedRuleset::Handle>(service->ruleset_dealer());
  AsyncDocumentSubresourceFilter::InitializationParams params(
      GURL("https://example.com/"), ActivationLevel::ENABLED, false);

  testing::TestActivationStateCallbackReceiver receiver;
  AsyncDocumentSubresourceFilter filter(ruleset_handle.get(), std::move(params),
                                        receiver.GetCallback());
  receiver.WaitForActivationDecision();
  receiver.ExpectReceivedOnce(ActivationState(ActivationLevel::DISABLED));
  RulesetVerificationStatus dealer_status = GetRulesetVerification();
  EXPECT_EQ(RulesetVerificationStatus::kCorrupt, dealer_status);
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       InvalidRuleset_NoActivation) {
  const char kTestRulesetSuffix[] = "foo";
  const int kNumberOfRules = 500;
  TestRulesetCreator ruleset_creator;
  TestRulesetPair test_ruleset_pair;
  ASSERT_NO_FATAL_FAILURE(
      ruleset_creator.CreateRulesetToDisallowURLsWithManySuffixes(
          kTestRulesetSuffix, kNumberOfRules, &test_ruleset_pair));
  testing::TestRuleset::CorruptByTruncating(test_ruleset_pair.indexed, 123);

  // Just publish the corrupt indexed file directly, to simulate it being
  // corrupt on startup.
  ContentRulesetService* service =
      g_browser_process->subresource_filter_ruleset_service();
  ASSERT_TRUE(service->ruleset_dealer());
  OpenAndPublishRuleset(service, test_ruleset_pair.indexed.path);

  auto ruleset_handle =
      std::make_unique<VerifiedRuleset::Handle>(service->ruleset_dealer());
  AsyncDocumentSubresourceFilter::InitializationParams params(
      GURL("https://example.com/"), ActivationLevel::ENABLED, false);

  testing::TestActivationStateCallbackReceiver receiver;
  AsyncDocumentSubresourceFilter filter(ruleset_handle.get(), std::move(params),
                                        receiver.GetCallback());
  receiver.WaitForActivationDecision();
  receiver.ExpectReceivedOnce(ActivationState(ActivationLevel::DISABLED));
  RulesetVerificationStatus dealer_status = GetRulesetVerification();
  EXPECT_EQ(RulesetVerificationStatus::kCorrupt, dealer_status);
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest, LazyRulesetValidation) {
  // The ruleset shouldn't be validated until it's used, unless ad tagging is
  // enabled.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(subresource_filter::kAdTagging);
  SetRulesetToDisallowURLsWithPathSuffix("included_script.js");
  RulesetVerificationStatus dealer_status = GetRulesetVerification();
  EXPECT_EQ(RulesetVerificationStatus::kNotVerified, dealer_status);
}

IN_PROC_BROWSER_TEST_F(SubresourceFilterBrowserTest,
                       AdsTaggingImmediateRulesetValidation) {
  // When Ads Tagging is enabled, the ruleset should be validated as soon as
  // it's published.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(subresource_filter::kAdTagging);

  SetRulesetToDisallowURLsWithPathSuffix("included_script.js");
  RulesetVerificationStatus dealer_status = GetRulesetVerification();
  EXPECT_EQ(RulesetVerificationStatus::kIntact, dealer_status);
}

}  // namespace subresource_filter
