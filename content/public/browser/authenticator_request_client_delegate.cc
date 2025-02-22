// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/authenticator_request_client_delegate.h"

#include <utility>

#include "base/callback.h"
#include "base/strings/string_piece.h"

namespace content {

AuthenticatorRequestClientDelegate::AuthenticatorRequestClientDelegate() =
    default;
AuthenticatorRequestClientDelegate::~AuthenticatorRequestClientDelegate() =
    default;

void AuthenticatorRequestClientDelegate::RegisterActionCallbacks(
    base::OnceClosure cancel_callback,
    device::FidoRequestHandlerBase::RequestCallback request_callback) {}

bool AuthenticatorRequestClientDelegate::ShouldPermitIndividualAttestation(
    const std::string& relying_party_id) {
  return false;
}

void AuthenticatorRequestClientDelegate::ShouldReturnAttestation(
    const std::string& relying_party_id,
    base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(true);
}

bool AuthenticatorRequestClientDelegate::IsFocused() {
  return true;
}

#if defined(OS_MACOSX)
base::Optional<AuthenticatorRequestClientDelegate::TouchIdAuthenticatorConfig>
AuthenticatorRequestClientDelegate::GetTouchIdAuthenticatorConfig() const {
  return base::nullopt;
}
#endif

void AuthenticatorRequestClientDelegate::UpdateLastTransportUsed(
    device::FidoTransportProtocol transport) {}

void AuthenticatorRequestClientDelegate::OnTransportAvailabilityEnumerated(
    device::FidoRequestHandlerBase::TransportAvailabilityInfo data) {}

bool AuthenticatorRequestClientDelegate::EmbedderControlsAuthenticatorDispatch(
    const device::FidoAuthenticator& authenticator) {
  return false;
}

void AuthenticatorRequestClientDelegate::BluetoothAdapterPowerChanged(
    bool is_powered_on) {}

void AuthenticatorRequestClientDelegate::FidoAuthenticatorAdded(
    const device::FidoAuthenticator& authenticator) {}

void AuthenticatorRequestClientDelegate::FidoAuthenticatorRemoved(
    base::StringPiece device_id) {}

}  // namespace content
