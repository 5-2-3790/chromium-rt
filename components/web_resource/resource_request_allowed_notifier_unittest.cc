// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "components/prefs/testing_pref_service.h"
#include "components/web_resource/eula_accepted_notifier.h"
#include "components/web_resource/resource_request_allowed_notifier_test_util.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_resource {

// EulaAcceptedNotifier test class that allows mocking the EULA accepted state
// and issuing simulated notifications.
class TestEulaAcceptedNotifier : public EulaAcceptedNotifier {
 public:
  TestEulaAcceptedNotifier()
      : EulaAcceptedNotifier(nullptr),
        eula_accepted_(false) {
  }
  ~TestEulaAcceptedNotifier() override {}

  bool IsEulaAccepted() override { return eula_accepted_; }

  void SetEulaAcceptedForTesting(bool eula_accepted) {
    eula_accepted_ = eula_accepted;
  }

  void SimulateEulaAccepted() {
    NotifyObserver();
  }

 private:
  bool eula_accepted_;

  DISALLOW_COPY_AND_ASSIGN(TestEulaAcceptedNotifier);
};

// A test fixture class for ResourceRequestAllowedNotifier tests that require
// network state simulations. This also acts as the service implementing the
// ResourceRequestAllowedNotifier::Observer interface.
class ResourceRequestAllowedNotifierTest
    : public testing::Test,
      public ResourceRequestAllowedNotifier::Observer {
 public:
  ResourceRequestAllowedNotifierTest()
      : network_tracker_(true,
                         network::mojom::ConnectionType::CONNECTION_UNKNOWN),
        resource_request_allowed_notifier_(&prefs_, &network_tracker_),
        eula_notifier_(new TestEulaAcceptedNotifier),
        was_notified_(false) {
    resource_request_allowed_notifier_.InitWithEulaAcceptNotifier(
        this, base::WrapUnique(eula_notifier_));
  }
  ~ResourceRequestAllowedNotifierTest() override {}

  bool was_notified() const { return was_notified_; }

  // ResourceRequestAllowedNotifier::Observer override:
  void OnResourceRequestsAllowed() override { was_notified_ = true; }

  void SimulateNetworkConnectionChange(network::mojom::ConnectionType type) {
    network_tracker_.SetConnectionType(type);
    base::RunLoop().RunUntilIdle();
  }

  // Simulate a resource request from the test service. It returns true if
  // resource request is allowed. Otherwise returns false and will change the
  // result of was_notified() to true when the request is allowed.
  bool SimulateResourceRequest() {
    return resource_request_allowed_notifier_.ResourceRequestsAllowed();
  }

  void SimulateEulaAccepted() {
    eula_notifier_->SimulateEulaAccepted();
  }

  // Eula manipulation methods:
  void SetNeedsEulaAcceptance(bool needs_acceptance) {
    eula_notifier_->SetEulaAcceptedForTesting(!needs_acceptance);
  }

  void SetWaitingForEula(bool waiting) {
    resource_request_allowed_notifier_.SetWaitingForEulaForTesting(waiting);
  }

  // Used in tests involving the EULA. Disables both the EULA accepted state
  // and the network.
  void DisableEulaAndNetwork() {
    SimulateNetworkConnectionChange(
        network::mojom::ConnectionType::CONNECTION_NONE);
    SetWaitingForEula(true);
    SetNeedsEulaAcceptance(true);
  }

  void SetUp() override {
    // Assume the test service has already requested permission, as all tests
    // just test that criteria changes notify the server.
    // Set default EULA state to done (not waiting and EULA accepted) to
    // simplify non-ChromeOS tests.
    SetWaitingForEula(false);
    SetNeedsEulaAcceptance(false);
  }

 private:
  base::MessageLoopForUI message_loop_;
  network::TestNetworkConnectionTracker network_tracker_;
  TestingPrefServiceSimple prefs_;
  TestRequestAllowedNotifier resource_request_allowed_notifier_;
  TestEulaAcceptedNotifier* eula_notifier_;  // Weak, owned by RRAN.
  bool was_notified_;

  DISALLOW_COPY_AND_ASSIGN(ResourceRequestAllowedNotifierTest);
};

TEST_F(ResourceRequestAllowedNotifierTest, DoNotNotifyIfOffline) {
  SimulateNetworkConnectionChange(
      network::mojom::ConnectionType::CONNECTION_NONE);
  EXPECT_FALSE(SimulateResourceRequest());

  SimulateNetworkConnectionChange(
      network::mojom::ConnectionType::CONNECTION_NONE);
  EXPECT_FALSE(was_notified());
}

TEST_F(ResourceRequestAllowedNotifierTest, DoNotNotifyIfOnlineToOnline) {
  SimulateNetworkConnectionChange(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  EXPECT_TRUE(SimulateResourceRequest());

  SimulateNetworkConnectionChange(
      network::mojom::ConnectionType::CONNECTION_ETHERNET);
  EXPECT_FALSE(was_notified());
}

TEST_F(ResourceRequestAllowedNotifierTest, NotifyOnReconnect) {
  SimulateNetworkConnectionChange(
      network::mojom::ConnectionType::CONNECTION_NONE);
  EXPECT_FALSE(SimulateResourceRequest());

  SimulateNetworkConnectionChange(
      network::mojom::ConnectionType::CONNECTION_ETHERNET);
  EXPECT_TRUE(was_notified());
}

TEST_F(ResourceRequestAllowedNotifierTest, NoNotifyOnWardriving) {
  SimulateNetworkConnectionChange(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  EXPECT_TRUE(SimulateResourceRequest());

  SimulateNetworkConnectionChange(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  EXPECT_FALSE(was_notified());
  SimulateNetworkConnectionChange(
      network::mojom::ConnectionType::CONNECTION_3G);
  EXPECT_FALSE(was_notified());
  SimulateNetworkConnectionChange(
      network::mojom::ConnectionType::CONNECTION_4G);
  EXPECT_FALSE(was_notified());
  SimulateNetworkConnectionChange(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  EXPECT_FALSE(was_notified());
}

TEST_F(ResourceRequestAllowedNotifierTest, NoNotifyOnFlakyConnection) {
  SimulateNetworkConnectionChange(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  EXPECT_TRUE(SimulateResourceRequest());

  SimulateNetworkConnectionChange(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  EXPECT_FALSE(was_notified());
  SimulateNetworkConnectionChange(
      network::mojom::ConnectionType::CONNECTION_NONE);
  EXPECT_FALSE(was_notified());
  SimulateNetworkConnectionChange(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  EXPECT_FALSE(was_notified());
}

TEST_F(ResourceRequestAllowedNotifierTest, NotifyOnFlakyConnection) {
  // First, the observer queries the state while the network is connected.
  SimulateNetworkConnectionChange(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  EXPECT_TRUE(SimulateResourceRequest());

  SimulateNetworkConnectionChange(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  EXPECT_FALSE(was_notified());
  SimulateNetworkConnectionChange(
      network::mojom::ConnectionType::CONNECTION_NONE);
  EXPECT_FALSE(was_notified());

  // Now, the observer queries the state while the network is disconnected.
  EXPECT_FALSE(SimulateResourceRequest());

  SimulateNetworkConnectionChange(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  EXPECT_TRUE(was_notified());
}

TEST_F(ResourceRequestAllowedNotifierTest, NoNotifyOnEulaAfterGoOffline) {
  DisableEulaAndNetwork();
  EXPECT_FALSE(SimulateResourceRequest());

  SimulateNetworkConnectionChange(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  EXPECT_FALSE(was_notified());
  SimulateNetworkConnectionChange(
      network::mojom::ConnectionType::CONNECTION_NONE);
  EXPECT_FALSE(was_notified());
  SimulateEulaAccepted();
  EXPECT_FALSE(was_notified());
}

TEST_F(ResourceRequestAllowedNotifierTest, NoRequestNoNotify) {
  // Ensure that if the observing service does not request access, it does not
  // get notified, even if the criteria are met. Note that this is done by not
  // calling SimulateResourceRequest here.
  SimulateNetworkConnectionChange(
      network::mojom::ConnectionType::CONNECTION_NONE);
  SimulateNetworkConnectionChange(
      network::mojom::ConnectionType::CONNECTION_ETHERNET);
  EXPECT_FALSE(was_notified());
}

TEST_F(ResourceRequestAllowedNotifierTest, EulaOnlyNetworkOffline) {
  DisableEulaAndNetwork();
  EXPECT_FALSE(SimulateResourceRequest());

  SimulateEulaAccepted();
  EXPECT_FALSE(was_notified());
}

TEST_F(ResourceRequestAllowedNotifierTest, EulaFirst) {
  DisableEulaAndNetwork();
  EXPECT_FALSE(SimulateResourceRequest());

  SimulateEulaAccepted();
  EXPECT_FALSE(was_notified());

  SimulateNetworkConnectionChange(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  EXPECT_TRUE(was_notified());
}

TEST_F(ResourceRequestAllowedNotifierTest, NetworkFirst) {
  DisableEulaAndNetwork();
  EXPECT_FALSE(SimulateResourceRequest());

  SimulateNetworkConnectionChange(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  EXPECT_FALSE(was_notified());

  SimulateEulaAccepted();
  EXPECT_TRUE(was_notified());
}

TEST_F(ResourceRequestAllowedNotifierTest, NoRequestNoNotifyEula) {
  // Ensure that if the observing service does not request access, it does not
  // get notified, even if the criteria are met. Note that this is done by not
  // calling SimulateResourceRequest here.
  DisableEulaAndNetwork();

  SimulateNetworkConnectionChange(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  EXPECT_FALSE(was_notified());

  SimulateEulaAccepted();
  EXPECT_FALSE(was_notified());
}

}  // namespace web_resource
