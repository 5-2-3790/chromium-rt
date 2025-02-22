// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_CHROMOTING_CLIENT_RUNTIME_H_
#define REMOTING_CLIENT_CHROMOTING_CLIENT_RUNTIME_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "net/url_request/url_request_context_getter.h"
#include "remoting/base/auto_thread.h"
#include "remoting/base/oauth_token_getter.h"
#include "remoting/base/telemetry_log_writer.h"

namespace base {
class MessageLoopForUI;

template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace network {
class SharedURLLoaderFactory;
class TransitionalURLLoaderFactoryOwner;
}  // namespace network

// Houses the global resources on which the Chromoting components run
// (e.g. message loops and task runners).
namespace remoting {

class ChromotingClientRuntime {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    // RuntimeWillShutdown will be called on the delegate when the runtime
    // enters into the destructor. This is a good time for the delegate to
    // start shutting down on threads while they exist.
    virtual void RuntimeWillShutdown() = 0;

    // RuntimeDidShutdown will be called after task managers and threads
    // have been stopped.
    virtual void RuntimeDidShutdown() = 0;

    // For fetching auth token. Called on the UI thread.
    virtual base::WeakPtr<OAuthTokenGetter> oauth_token_getter() = 0;
  };

  static ChromotingClientRuntime* GetInstance();

  // Must be called before calling any other methods on this object.
  void Init(ChromotingClientRuntime::Delegate* delegate);

  std::unique_ptr<OAuthTokenGetter> CreateOAuthTokenGetter();

  scoped_refptr<AutoThreadTaskRunner> network_task_runner() {
    return network_task_runner_;
  }

  scoped_refptr<AutoThreadTaskRunner> audio_task_runner() {
    return audio_task_runner_;
  }

  scoped_refptr<AutoThreadTaskRunner> ui_task_runner() {
    return ui_task_runner_;
  }

  scoped_refptr<AutoThreadTaskRunner> display_task_runner() {
    return display_task_runner_;
  }

  scoped_refptr<net::URLRequestContextGetter> url_requester() {
    return url_requester_;
  }

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory();

  ChromotingEventLogWriter* log_writer() { return log_writer_.get(); }

 private:
  ChromotingClientRuntime();
  virtual ~ChromotingClientRuntime();

  // Chromium code's connection to the app message loop. Once created the
  // MessageLoop will live for the life of the program.
  std::unique_ptr<base::MessageLoopForUI> ui_loop_;

  // References to native threads.
  scoped_refptr<AutoThreadTaskRunner> ui_task_runner_;

  // TODO(nicholss): AutoThreads will be leaked because they depend on the main
  // thread. We should update this class to use regular threads like the client
  // plugin does.
  // Longer term we should migrate most of these to background tasks except the
  // network thread to TaskScheduler, removing the need for threads.

  scoped_refptr<AutoThreadTaskRunner> audio_task_runner_;
  scoped_refptr<AutoThreadTaskRunner> display_task_runner_;
  scoped_refptr<AutoThreadTaskRunner> network_task_runner_;

  scoped_refptr<net::URLRequestContextGetter> url_requester_;
  std::unique_ptr<network::TransitionalURLLoaderFactoryOwner>
      url_loader_factory_owner_;

  // For logging session stage changes and stats.
  std::unique_ptr<TelemetryLogWriter> log_writer_;

  ChromotingClientRuntime::Delegate* delegate_ = nullptr;

  friend struct base::DefaultSingletonTraits<ChromotingClientRuntime>;

  DISALLOW_COPY_AND_ASSIGN(ChromotingClientRuntime);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_CHROMOTING_CLIENT_RUNTIME_H_
