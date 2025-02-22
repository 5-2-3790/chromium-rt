// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_P2P_SOCKET_TEST_UTILS_H_
#define SERVICES_NETWORK_P2P_SOCKET_TEST_UTILS_H_

#include <stdint.h>

#include <string>
#include <tuple>
#include <vector>

#include "mojo/public/cpp/bindings/binding.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/stream_socket.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/p2p_param_traits.h"
#include "services/network/public/mojom/p2p.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

const char kTestLocalIpAddress[] = "123.44.22.4";
const char kTestIpAddress1[] = "123.44.22.31";
const uint16_t kTestPort1 = 234;
const char kTestIpAddress2[] = "133.11.22.33";
const uint16_t kTestPort2 = 543;

class FakeSocket : public net::StreamSocket {
 public:
  FakeSocket(std::string* written_data);
  ~FakeSocket() override;

  void set_async_write(bool async_write) { async_write_ = async_write; }
  void AppendInputData(const char* data, int data_size);
  int input_pos() const { return input_pos_; }
  bool read_pending() const { return read_pending_; }
  void SetPeerAddress(const net::IPEndPoint& peer_address);
  void SetLocalAddress(const net::IPEndPoint& local_address);

  // net::Socket implementation.
  int Read(net::IOBuffer* buf,
           int buf_len,
           net::CompletionOnceCallback callback) override;
  int Write(
      net::IOBuffer* buf,
      int buf_len,
      net::CompletionOnceCallback callback,
      const net::NetworkTrafficAnnotationTag& traffic_annotation) override;
  int SetReceiveBufferSize(int32_t size) override;
  int SetSendBufferSize(int32_t size) override;
  int Connect(net::CompletionOnceCallback callback) override;
  void Disconnect() override;
  bool IsConnected() const override;
  bool IsConnectedAndIdle() const override;
  int GetPeerAddress(net::IPEndPoint* address) const override;
  int GetLocalAddress(net::IPEndPoint* address) const override;
  const net::NetLogWithSource& NetLog() const override;
  bool WasEverUsed() const override;
  bool WasAlpnNegotiated() const override;
  net::NextProto GetNegotiatedProtocol() const override;
  bool GetSSLInfo(net::SSLInfo* ssl_info) override;
  void GetConnectionAttempts(net::ConnectionAttempts* out) const override;
  void ClearConnectionAttempts() override {}
  void AddConnectionAttempts(const net::ConnectionAttempts& attempts) override {
  }
  int64_t GetTotalReceivedBytes() const override;
  void ApplySocketTag(const net::SocketTag& tag) override {}

 private:
  void DoAsyncWrite(scoped_refptr<net::IOBuffer> buf,
                    int buf_len,
                    net::CompletionOnceCallback callback);

  bool read_pending_;
  scoped_refptr<net::IOBuffer> read_buffer_;
  int read_buffer_size_;
  net::CompletionOnceCallback read_callback_;

  std::string input_data_;
  int input_pos_;

  std::string* written_data_;
  bool async_write_;
  bool write_pending_;

  net::IPEndPoint peer_address_;
  net::IPEndPoint local_address_;

  net::NetLogWithSource net_log_;
};

class FakeSocketClient : public mojom::P2PSocketClient {
 public:
  FakeSocketClient(mojom::P2PSocketPtr socket,
                   mojom::P2PSocketClientRequest client_request);
  ~FakeSocketClient() override;

  // mojom::P2PSocketClient interface.
  MOCK_METHOD2(SocketCreated,
               void(const net::IPEndPoint&, const net::IPEndPoint&));
  MOCK_METHOD1(SendComplete, void(const P2PSendPacketMetrics&));
  MOCK_METHOD1(IncomingTcpConnection, void(const net::IPEndPoint&));
  MOCK_METHOD3(DataReceived,
               void(const net::IPEndPoint&,
                    const std::vector<int8_t>&,
                    base::TimeTicks));

  bool connection_error() { return connection_error_; }

 private:
  mojom::P2PSocketPtr socket_;
  mojo::Binding<mojom::P2PSocketClient> binding_;
  bool connection_error_ = false;
};

void CreateRandomPacket(std::vector<int8_t>* packet);
void CreateStunRequest(std::vector<int8_t>* packet);
void CreateStunResponse(std::vector<int8_t>* packet);
void CreateStunError(std::vector<int8_t>* packet);

net::IPEndPoint ParseAddress(const std::string& ip_str, uint16_t port);

MATCHER_P(MatchMessage, type, "") {
  return arg->type() == type;
}

MATCHER_P2(MatchSendPacketMetrics, rtc_packet_id, test_start_time, "") {
  return arg.rtc_packet_id == rtc_packet_id &&
         arg.send_time >= test_start_time &&
         arg.send_time <= base::TimeTicks::Now();
}

}  // namespace network

#endif  // SERVICES_NETWORK_P2P_SOCKET_TEST_UTILS_H_
