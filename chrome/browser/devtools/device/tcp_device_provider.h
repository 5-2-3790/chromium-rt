// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVICE_TCP_DEVICE_PROVIDER_H_
#define CHROME_BROWSER_DEVTOOLS_DEVICE_TCP_DEVICE_PROVIDER_H_

#include <stdint.h>

#include <set>

#include "chrome/browser/devtools/device/android_device_manager.h"
#include "net/base/host_port_pair.h"

// Instantiate this class only in a test and/or when the DEBUG_DEVTOOLS
// BUILDFLAG is set.
class TCPDeviceProvider : public AndroidDeviceManager::DeviceProvider {
 public:
  static scoped_refptr<TCPDeviceProvider> CreateForLocalhost(
      uint16_t port,
      content::BrowserContext* context);

  using HostPortSet = std::set<net::HostPortPair>;
  TCPDeviceProvider(const HostPortSet& targets,
                    content::BrowserContext* context);

  void QueryDevices(const SerialsCallback& callback) override;

  void QueryDeviceInfo(const std::string& serial,
                       const DeviceInfoCallback& callback) override;

  void OpenSocket(const std::string& serial,
                  const std::string& socket_name,
                  const SocketCallback& callback) override;

  void ReleaseDevice(const std::string& serial) override;

  void set_targets(HostPortSet targets) { targets_ = std::move(targets); }

  void set_release_callback_for_test(const base::Closure& callback);

  HostPortSet get_targets_for_test() { return targets_; }

 private:
  ~TCPDeviceProvider() override;

  void InitializeHostResolver();
  void InitializeHostResolverOnUI(network::mojom::HostResolverRequest request);

  HostPortSet targets_;
  base::Closure release_callback_;
  content::BrowserContext* context_;
  network::mojom::HostResolverPtr host_resolver_;
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVICE_TCP_DEVICE_PROVIDER_H_
