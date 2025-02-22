// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/net/http_bridge.h"

#include <stddef.h>

#include <utility>

#include "base/bit_cast.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "components/sync/base/cancelation_signal.h"
#include "net/http/http_response_headers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/compression_utils.h"

namespace syncer {

namespace {

// TODO(timsteele): Should use PathService here. See Chromium Issue 3113.
const base::FilePath::CharType kDocRoot[] =
    FILE_PATH_LITERAL("chrome/test/data");

}  // namespace

const char kUserAgent[] = "user-agent";

#if defined(OS_ANDROID)
#define MAYBE_SyncHttpBridgeTest DISABLED_SyncHttpBridgeTest
#else
#define MAYBE_SyncHttpBridgeTest SyncHttpBridgeTest
#endif  // defined(OS_ANDROID)
class MAYBE_SyncHttpBridgeTest : public testing::Test {
 public:
  MAYBE_SyncHttpBridgeTest()
      : bridge_for_race_test_(nullptr), io_thread_("IO thread") {
    test_server_.AddDefaultHandlers(base::FilePath(kDocRoot));
  }

  void SetUp() override {
    base::Thread::Options options;
    options.message_loop_type = base::MessageLoop::TYPE_IO;
    io_thread_.StartWithOptions(options);

    HttpBridge::SetIOCapableTaskRunnerForTest(io_thread_.task_runner());
  }

  void TearDown() override {
    io_thread_.Stop();
  }

  HttpBridge* BuildBridge() { return new CustomHttpBridge(); }

  static void Abort(HttpBridge* bridge) { bridge->Abort(); }

  // Used by AbortAndReleaseBeforeFetchCompletes to test an interesting race
  // condition.
  void RunSyncThreadBridgeUseTest(base::WaitableEvent* signal_when_created,
                                  base::WaitableEvent* signal_when_released);

  base::MessageLoop* GetIOThreadLoop() { return io_thread_.message_loop(); }

  net::EmbeddedTestServer test_server_;

  base::Thread* io_thread() { return &io_thread_; }

  HttpBridge* bridge_for_race_test() { return bridge_for_race_test_; }

 private:
  // A custom HTTPBridge implementation that sets a SharedURLLoaderFactory
  // instance from the IO-capable thread.
  class CustomHttpBridge : public HttpBridge {
   public:
    CustomHttpBridge()
        : HttpBridge(kUserAgent,
                     nullptr /*SharedURLLoaderFactoryInfo*/,
                     NetworkTimeUpdateCallback(),
                     BindToTrackerCallback()) {}

   protected:
    ~CustomHttpBridge() override {}

    void MakeAsynchronousPost() override {
      set_url_loader_factory_for_testing(
          base::MakeRefCounted<network::TestSharedURLLoaderFactory>());

      HttpBridge::MakeAsynchronousPost();

      // Attempt to spin a loop so that mojom::URLLoaderFactory get executed.
      base::RunLoop().RunUntilIdle();
    }
  };

  HttpBridge* bridge_for_race_test_;

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  // Separate thread for IO used by the HttpBridge.
  base::Thread io_thread_;
};

// An HttpBridge that doesn't actually make network requests and just calls
// back with dummy response info.
// TODO(tim): Instead of inheriting here we should inject a component
// responsible for the MakeAsynchronousPost bit.
class ShuntedHttpBridge : public HttpBridge {
 public:
  // If |never_finishes| is true, the simulated request never actually
  // returns.
  ShuntedHttpBridge(MAYBE_SyncHttpBridgeTest* test, bool never_finishes)
      : HttpBridge(
            kUserAgent,
            nullptr /*SharedURLLoaderFactoryInfo, unneeded as we mock stuff*/,
            NetworkTimeUpdateCallback(),
            BindToTrackerCallback()),
        test_(test),
        never_finishes_(never_finishes) {}

 protected:
  void MakeAsynchronousPost() override {
    ASSERT_TRUE(
        test_->GetIOThreadLoop()->task_runner()->BelongsToCurrentThread());
    if (never_finishes_)
      return;

    // We don't actually want to make a request for this test, so just callback
    // as if it completed.
    test_->GetIOThreadLoop()->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&ShuntedHttpBridge::CallOnURLFetchComplete, this));
  }

 private:
  ~ShuntedHttpBridge() override {}

  void CallOnURLFetchComplete() {
    ASSERT_TRUE(
        test_->GetIOThreadLoop()->task_runner()->BelongsToCurrentThread());

    // Set up a dummy content response.
    OnURLLoadCompleteInternal(200, net::OK, 0 /* content length, irrelevant */,
                              GURL("http://www.google.com"),
                              std::make_unique<std::string>("success!"));
  }
  MAYBE_SyncHttpBridgeTest* test_;
  bool never_finishes_;
};

void MAYBE_SyncHttpBridgeTest::RunSyncThreadBridgeUseTest(
    base::WaitableEvent* signal_when_created,
    base::WaitableEvent* signal_when_released) {
  {
    scoped_refptr<ShuntedHttpBridge> bridge(new ShuntedHttpBridge(this, true));
    bridge->SetURL("http://www.google.com", 9999);
    bridge->SetPostPayload("text/plain", 2, " ");
    bridge_for_race_test_ = bridge.get();
    signal_when_created->Signal();

    int os_error = 0;
    int response_code = 0;
    bridge->MakeSynchronousPost(&os_error, &response_code);
    bridge_for_race_test_ = nullptr;
  }
  signal_when_released->Signal();
}

// Test the HttpBridge without actually making any network requests.
TEST_F(MAYBE_SyncHttpBridgeTest, TestMakeSynchronousPostShunted) {
  scoped_refptr<HttpBridge> http_bridge(new ShuntedHttpBridge(this, false));
  http_bridge->SetURL("http://www.google.com", 9999);
  http_bridge->SetPostPayload("text/plain", 2, " ");

  int os_error = 0;
  int response_code = 0;
  bool success = http_bridge->MakeSynchronousPost(&os_error, &response_code);
  EXPECT_TRUE(success);
  EXPECT_EQ(200, response_code);
  EXPECT_EQ(0, os_error);

  EXPECT_EQ(8, http_bridge->GetResponseContentLength());
  EXPECT_EQ(std::string("success!"),
            std::string(http_bridge->GetResponseContent()));
}

// Full round-trip test of the HttpBridge with compressed data, check if the
// data is correctly compressed.
TEST_F(MAYBE_SyncHttpBridgeTest, CompressedRequestPayloadCheck) {
  ASSERT_TRUE(test_server_.Start());

  scoped_refptr<HttpBridge> http_bridge(BuildBridge());

  std::string payload =
      "this should be echoed back, this should be echoed back.";
  GURL echo = test_server_.GetURL("/echo");
  http_bridge->SetURL(echo.spec().c_str(), echo.IntPort());
  http_bridge->SetPostPayload("application/x-www-form-urlencoded",
                              payload.length(), payload.c_str());
  int os_error = 0;
  int response_code = 0;
  bool success = http_bridge->MakeSynchronousPost(&os_error, &response_code);
  EXPECT_TRUE(success);
  EXPECT_EQ(200, response_code);
  EXPECT_EQ(0, os_error);

  // Verifying compression, check if the actual payload is compressed correctly.
  EXPECT_GT(payload.length(),
            static_cast<size_t>(http_bridge->GetResponseContentLength()));
  std::string compressed_payload(http_bridge->GetResponseContent(),
                                 http_bridge->GetResponseContentLength());
  std::string uncompressed_payload;
  compression::GzipUncompress(compressed_payload, &uncompressed_payload);
  EXPECT_EQ(payload, uncompressed_payload);
}

// Full round-trip test of the HttpBridge with compression, check if header
// fields("Content-Encoding" ,"Accept-Encoding" and user agent) are set
// correctly.
TEST_F(MAYBE_SyncHttpBridgeTest, CompressedRequestHeaderCheck) {
  ASSERT_TRUE(test_server_.Start());

  scoped_refptr<HttpBridge> http_bridge(BuildBridge());

  GURL echo_header = test_server_.GetURL("/echoall");
  http_bridge->SetURL(echo_header.spec().c_str(), echo_header.IntPort());

  std::string test_payload = "###TEST PAYLOAD###";
  http_bridge->SetPostPayload("text/html", test_payload.length() + 1,
                              test_payload.c_str());

  int os_error = 0;
  int response_code = 0;
  bool success = http_bridge->MakeSynchronousPost(&os_error, &response_code);
  EXPECT_TRUE(success);
  EXPECT_EQ(200, response_code);
  EXPECT_EQ(0, os_error);

  std::string response(http_bridge->GetResponseContent(),
                       http_bridge->GetResponseContentLength());
  EXPECT_NE(std::string::npos, response.find("Content-Encoding: gzip"));
  EXPECT_NE(std::string::npos,
            response.find(base::StringPrintf(
                "%s: %s", net::HttpRequestHeaders::kAcceptEncoding,
                "gzip, deflate")));
  EXPECT_NE(std::string::npos,
            response.find(base::StringPrintf(
                "%s: %s", net::HttpRequestHeaders::kUserAgent, kUserAgent)));
}

TEST_F(MAYBE_SyncHttpBridgeTest, TestExtraRequestHeaders) {
  ASSERT_TRUE(test_server_.Start());

  scoped_refptr<HttpBridge> http_bridge(BuildBridge());

  GURL echo_header = test_server_.GetURL("/echoall");

  http_bridge->SetURL(echo_header.spec().c_str(), echo_header.IntPort());
  http_bridge->SetExtraRequestHeaders("test:fnord");

  std::string test_payload = "###TEST PAYLOAD###";
  http_bridge->SetPostPayload("text/html", test_payload.length() + 1,
                              test_payload.c_str());

  int os_error = 0;
  int response_code = 0;
  bool success = http_bridge->MakeSynchronousPost(&os_error, &response_code);
  EXPECT_TRUE(success);
  EXPECT_EQ(200, response_code);
  EXPECT_EQ(0, os_error);

  std::string response(http_bridge->GetResponseContent(),
                       http_bridge->GetResponseContentLength());

  EXPECT_NE(std::string::npos, response.find("fnord"));
}

TEST_F(MAYBE_SyncHttpBridgeTest, TestResponseHeader) {
  ASSERT_TRUE(test_server_.Start());

  scoped_refptr<HttpBridge> http_bridge(BuildBridge());

  GURL echo_header = test_server_.GetURL("/echoall");
  http_bridge->SetURL(echo_header.spec().c_str(), echo_header.IntPort());

  std::string test_payload = "###TEST PAYLOAD###";
  http_bridge->SetPostPayload("text/html", test_payload.length() + 1,
                              test_payload.c_str());

  int os_error = 0;
  int response_code = 0;
  bool success = http_bridge->MakeSynchronousPost(&os_error, &response_code);
  EXPECT_TRUE(success);
  EXPECT_EQ(200, response_code);
  EXPECT_EQ(0, os_error);

  EXPECT_EQ(http_bridge->GetResponseHeaderValue("Content-type"), "text/html");
  EXPECT_TRUE(http_bridge->GetResponseHeaderValue("invalid-header").empty());
}

TEST_F(MAYBE_SyncHttpBridgeTest, Abort) {
  scoped_refptr<ShuntedHttpBridge> http_bridge(
      new ShuntedHttpBridge(this, true));
  http_bridge->SetURL("http://www.google.com", 9999);
  http_bridge->SetPostPayload("text/plain", 2, " ");

  int os_error = 0;
  int response_code = 0;

  io_thread()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&MAYBE_SyncHttpBridgeTest::Abort,
                                base::RetainedRef(http_bridge)));
  bool success = http_bridge->MakeSynchronousPost(&os_error, &response_code);
  EXPECT_FALSE(success);
  EXPECT_EQ(net::ERR_ABORTED, os_error);
}

TEST_F(MAYBE_SyncHttpBridgeTest, AbortLate) {
  scoped_refptr<ShuntedHttpBridge> http_bridge(
      new ShuntedHttpBridge(this, false));
  http_bridge->SetURL("http://www.google.com", 9999);
  http_bridge->SetPostPayload("text/plain", 2, " ");

  int os_error = 0;
  int response_code = 0;

  bool success = http_bridge->MakeSynchronousPost(&os_error, &response_code);
  ASSERT_TRUE(success);
  http_bridge->Abort();
  // Ensures no double-free of URLFetcher, etc.
}

// Tests an interesting case where code using the HttpBridge aborts the fetch
// and releases ownership before a pending fetch completed callback is issued by
// the underlying URLFetcher (and before that URLFetcher is destroyed, which
// would cancel the callback).
TEST_F(MAYBE_SyncHttpBridgeTest, AbortAndReleaseBeforeFetchComplete) {
  base::Thread sync_thread("SyncThread");
  sync_thread.Start();

  // First, block the sync thread on the post.
  base::WaitableEvent signal_when_created(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  base::WaitableEvent signal_when_released(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  sync_thread.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&MAYBE_SyncHttpBridgeTest::RunSyncThreadBridgeUseTest,
                     base::Unretained(this), &signal_when_created,
                     &signal_when_released));

  // Stop IO so we can control order of operations.
  base::WaitableEvent io_waiter(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  ASSERT_TRUE(io_thread()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&base::WaitableEvent::Wait,
                                base::Unretained(&io_waiter))));

  signal_when_created.Wait();  // Wait till we have a bridge to abort.
  ASSERT_TRUE(bridge_for_race_test());

  // Schedule the fetch completion callback (but don't run it yet). Don't take
  // a reference to the bridge to mimic URLFetcher's handling of the delegate.
  ASSERT_TRUE(io_thread()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&syncer::HttpBridge::OnURLLoadCompleteInternal,
                                base::Unretained(bridge_for_race_test()), 200,
                                net::OK, 0, GURL("http://www.google.com"),
                                std::make_unique<std::string>("success!"))));

  // Abort the fetch. This should be smart enough to handle the case where
  // the bridge is destroyed before the callback scheduled above completes.
  bridge_for_race_test()->Abort();

  // Wait until the sync thread releases its ref on the bridge.
  signal_when_released.Wait();
  ASSERT_FALSE(bridge_for_race_test());

  // Unleash the hounds. The fetch completion callback should fire first, and
  // succeed even though we Release()d the bridge above because the call to
  // Abort should have held a reference.
  io_waiter.Signal();

  // Done.
  sync_thread.Stop();
  io_thread()->Stop();
}

void WaitOnIOThread(base::WaitableEvent* signal_wait_start,
                    base::WaitableEvent* wait_done) {
  signal_wait_start->Signal();
  wait_done->Wait();
}

}  // namespace syncer
