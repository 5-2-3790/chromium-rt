// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webauthn/sheet_models.h"

#include <vector>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/ui/webauthn/other_transports_menu_model.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

// AuthenticatorSheetModelBase ------------------------------------------------

AuthenticatorSheetModelBase::AuthenticatorSheetModelBase(
    AuthenticatorRequestDialogModel* dialog_model)
    : dialog_model_(dialog_model) {
  DCHECK(dialog_model);
  dialog_model_->AddObserver(this);
}

AuthenticatorSheetModelBase::~AuthenticatorSheetModelBase() {
  if (dialog_model_) {
    dialog_model_->RemoveObserver(this);
    dialog_model_ = nullptr;
  }
}

// static
gfx::ImageSkia* AuthenticatorSheetModelBase::GetImage(int resource_id) {
  return ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(resource_id);
}

bool AuthenticatorSheetModelBase::IsActivityIndicatorVisible() const {
  return false;
}

bool AuthenticatorSheetModelBase::IsBackButtonVisible() const {
  return true;
}

bool AuthenticatorSheetModelBase::IsCancelButtonVisible() const {
  return true;
}

base::string16 AuthenticatorSheetModelBase::GetCancelButtonLabel() const {
  return l10n_util::GetStringUTF16(IDS_CANCEL);
}

bool AuthenticatorSheetModelBase::IsAcceptButtonVisible() const {
  return false;
}

bool AuthenticatorSheetModelBase::IsAcceptButtonEnabled() const {
  NOTREACHED();
  return false;
}

base::string16 AuthenticatorSheetModelBase::GetAcceptButtonLabel() const {
  NOTREACHED();
  return base::string16();
}

ui::MenuModel* AuthenticatorSheetModelBase::GetOtherTransportsMenuModel() {
  return nullptr;
}

void AuthenticatorSheetModelBase::OnBack() {
  if (dialog_model())
    dialog_model()->Back();
}

void AuthenticatorSheetModelBase::OnAccept() {
  NOTREACHED();
}

void AuthenticatorSheetModelBase::OnCancel() {
  if (dialog_model())
    dialog_model()->Cancel();
}

base::string16 AuthenticatorSheetModelBase::GetRelyingPartyIdString() const {
  DCHECK(!dialog_model()->transport_availability()->rp_id.empty());
  return base::UTF8ToUTF16(dialog_model()->transport_availability()->rp_id);
}

void AuthenticatorSheetModelBase::OnModelDestroyed() {
  dialog_model_ = nullptr;
}

// AuthenticatorWelcomeSheetModel ---------------------------------------------

gfx::ImageSkia* AuthenticatorWelcomeSheetModel::GetStepIllustration() const {
  return GetImage(IDR_WEBAUTHN_ILLUSTRATION_WELCOME);
}

base::string16 AuthenticatorWelcomeSheetModel::GetStepTitle() const {
  return l10n_util::GetStringFUTF16(IDS_WEBAUTHN_WELCOME_SCREEN_TITLE,
                                    GetRelyingPartyIdString());
}

base::string16 AuthenticatorWelcomeSheetModel::GetStepDescription() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_WELCOME_SCREEN_DESCRIPTION);
}

bool AuthenticatorWelcomeSheetModel::IsAcceptButtonVisible() const {
  return true;
}

bool AuthenticatorWelcomeSheetModel::IsAcceptButtonEnabled() const {
  return true;
}

base::string16 AuthenticatorWelcomeSheetModel::GetAcceptButtonLabel() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_WELCOME_SCREEN_NEXT);
}

void AuthenticatorWelcomeSheetModel::OnAccept() {
  dialog_model()
      ->StartGuidedFlowForMostLikelyTransportOrShowTransportSelection();
}

// AuthenticatorTransportSelectorSheetModel -----------------------------------

gfx::ImageSkia* AuthenticatorTransportSelectorSheetModel::GetStepIllustration()
    const {
  return GetImage(IDR_WEBAUTHN_ILLUSTRATION_WELCOME);
}

base::string16 AuthenticatorTransportSelectorSheetModel::GetStepTitle() const {
  return l10n_util::GetStringFUTF16(IDS_WEBAUTHN_TRANSPORT_SELECTION_TITLE,
                                    GetRelyingPartyIdString());
}

base::string16 AuthenticatorTransportSelectorSheetModel::GetStepDescription()
    const {
  return l10n_util::GetStringUTF16(
      IDS_WEBAUTHN_TRANSPORT_SELECTION_DESCRIPTION);
}

void AuthenticatorTransportSelectorSheetModel::OnTransportSelected(
    AuthenticatorTransport transport) {
  dialog_model()->StartGuidedFlowForTransport(transport);
}

// AuthenticatorInsertAndActivateUsbSheetModel ----------------------

AuthenticatorInsertAndActivateUsbSheetModel::
    AuthenticatorInsertAndActivateUsbSheetModel(
        AuthenticatorRequestDialogModel* dialog_model)
    : AuthenticatorSheetModelBase(dialog_model) {
  other_transports_menu_model_ = std::make_unique<OtherTransportsMenuModel>(
      dialog_model, AuthenticatorTransport::kUsbHumanInterfaceDevice);
}

AuthenticatorInsertAndActivateUsbSheetModel::
    ~AuthenticatorInsertAndActivateUsbSheetModel() = default;

bool AuthenticatorInsertAndActivateUsbSheetModel::IsActivityIndicatorVisible()
    const {
  return true;
}

gfx::ImageSkia*
AuthenticatorInsertAndActivateUsbSheetModel::GetStepIllustration() const {
  return GetImage(IDR_WEBAUTHN_ILLUSTRATION_USB);
}

base::string16 AuthenticatorInsertAndActivateUsbSheetModel::GetStepTitle()
    const {
  return l10n_util::GetStringFUTF16(IDS_WEBAUTHN_GENERIC_TITLE,
                                    GetRelyingPartyIdString());
}

base::string16 AuthenticatorInsertAndActivateUsbSheetModel::GetStepDescription()
    const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_USB_ACTIVATE_DESCRIPTION);
}

ui::MenuModel*
AuthenticatorInsertAndActivateUsbSheetModel::GetOtherTransportsMenuModel() {
  return other_transports_menu_model_.get();
}

// AuthenticatorTimeoutErrorModel ---------------------------------------------

gfx::ImageSkia* AuthenticatorTimeoutErrorModel::GetStepIllustration() const {
  return GetImage(IDR_WEBAUTHN_ILLUSTRATION_ERROR);
}

base::string16 AuthenticatorTimeoutErrorModel::GetStepTitle() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_ERROR_GENERIC_TITLE);
}

base::string16 AuthenticatorTimeoutErrorModel::GetStepDescription() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_ERROR_TIMEOUT_DESCRIPTION);
}

// AuthenticatorNoAvailableTransportsErrorModel -------------------------------

bool AuthenticatorNoAvailableTransportsErrorModel::IsBackButtonVisible() const {
  return false;
}

base::string16
AuthenticatorNoAvailableTransportsErrorModel::GetCancelButtonLabel() const {
  return l10n_util::GetStringUTF16(IDS_CLOSE);
}

gfx::ImageSkia*
AuthenticatorNoAvailableTransportsErrorModel::GetStepIllustration() const {
  return GetImage(IDR_WEBAUTHN_ILLUSTRATION_ERROR);
}

base::string16 AuthenticatorNoAvailableTransportsErrorModel::GetStepTitle()
    const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_ERROR_NO_TRANSPORTS_TITLE);
}

base::string16
AuthenticatorNoAvailableTransportsErrorModel::GetStepDescription() const {
  return l10n_util::GetStringUTF16(
      IDS_WEBAUTHN_ERROR_NO_TRANSPORTS_DESCRIPTION);
}

// AuthenticatorNotRegisteredErrorModel ---------------------------------------

gfx::ImageSkia* AuthenticatorNotRegisteredErrorModel::GetStepIllustration()
    const {
  return GetImage(IDR_WEBAUTHN_ILLUSTRATION_ERROR);
}

base::string16 AuthenticatorNotRegisteredErrorModel::GetStepTitle() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_ERROR_WRONG_KEY_SIGN_TITLE);
}

base::string16 AuthenticatorNotRegisteredErrorModel::GetStepDescription()
    const {
  return l10n_util::GetStringUTF16(
      IDS_WEBAUTHN_ERROR_WRONG_KEY_SIGN_DESCRIPTION);
}

bool AuthenticatorNotRegisteredErrorModel::IsAcceptButtonVisible() const {
  return true;
}

bool AuthenticatorNotRegisteredErrorModel::IsAcceptButtonEnabled() const {
  return true;
}

base::string16 AuthenticatorNotRegisteredErrorModel::GetAcceptButtonLabel()
    const {
  // TODO(engedy): This should use a separate string resource.
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_BLUETOOTH_POWER_ON_MANUAL_NEXT);
}

void AuthenticatorNotRegisteredErrorModel::OnAccept() {}

// AuthenticatorAlreadyRegisteredErrorModel -----------------------------------

gfx::ImageSkia* AuthenticatorAlreadyRegisteredErrorModel::GetStepIllustration()
    const {
  return GetImage(IDR_WEBAUTHN_ILLUSTRATION_ERROR);
}

base::string16 AuthenticatorAlreadyRegisteredErrorModel::GetStepTitle() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_ERROR_WRONG_KEY_REGISTER_TITLE);
}

base::string16 AuthenticatorAlreadyRegisteredErrorModel::GetStepDescription()
    const {
  return l10n_util::GetStringUTF16(
      IDS_WEBAUTHN_ERROR_WRONG_KEY_REGISTER_DESCRIPTION);
}

bool AuthenticatorAlreadyRegisteredErrorModel::IsAcceptButtonVisible() const {
  return true;
}

bool AuthenticatorAlreadyRegisteredErrorModel::IsAcceptButtonEnabled() const {
  return true;
}

base::string16 AuthenticatorAlreadyRegisteredErrorModel::GetAcceptButtonLabel()
    const {
  // TODO(engedy): This should use a separate string resource.
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_BLUETOOTH_POWER_ON_MANUAL_NEXT);
}

void AuthenticatorAlreadyRegisteredErrorModel::OnAccept() {}

// AuthenticatorBlePowerOnManualSheetModel ------------------------------------

gfx::ImageSkia* AuthenticatorBlePowerOnManualSheetModel::GetStepIllustration()
    const {
  return GetImage(IDR_WEBAUTHN_ILLUSTRATION_ERROR_BLUETOOTH);
}

base::string16 AuthenticatorBlePowerOnManualSheetModel::GetStepTitle() const {
  return l10n_util::GetStringUTF16(
      IDS_WEBAUTHN_BLUETOOTH_POWER_ON_MANUAL_TITLE);
}

base::string16 AuthenticatorBlePowerOnManualSheetModel::GetStepDescription()
    const {
  return l10n_util::GetStringUTF16(
      IDS_WEBAUTHN_BLUETOOTH_POWER_ON_MANUAL_DESCRIPTION);
}

bool AuthenticatorBlePowerOnManualSheetModel::IsAcceptButtonVisible() const {
  return true;
}

bool AuthenticatorBlePowerOnManualSheetModel::IsAcceptButtonEnabled() const {
  return true;
}

base::string16 AuthenticatorBlePowerOnManualSheetModel::GetAcceptButtonLabel()
    const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_BLUETOOTH_POWER_ON_MANUAL_NEXT);
}

// AuthenticatorBlePairingBeginSheetModel -------------------------------------

gfx::ImageSkia* AuthenticatorBlePairingBeginSheetModel::GetStepIllustration()
    const {
  return GetImage(IDR_WEBAUTHN_ILLUSTRATION_BLE);
}

base::string16 AuthenticatorBlePairingBeginSheetModel::GetStepTitle() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_BLE_PAIRING_BEGIN_TITLE);
}

base::string16 AuthenticatorBlePairingBeginSheetModel::GetStepDescription()
    const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_BLE_PAIRING_BEGIN_DESCRIPTION);
}

bool AuthenticatorBlePairingBeginSheetModel::IsAcceptButtonVisible() const {
  return true;
}

bool AuthenticatorBlePairingBeginSheetModel::IsAcceptButtonEnabled() const {
  return true;
}

base::string16 AuthenticatorBlePairingBeginSheetModel::GetAcceptButtonLabel()
    const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_BLE_PAIRING_BEGIN_NEXT);
}

// AuthenticatorBleEnterPairingModeSheetModel ---------------------------------

gfx::ImageSkia*
AuthenticatorBleEnterPairingModeSheetModel::GetStepIllustration() const {
  return GetImage(IDR_WEBAUTHN_ILLUSTRATION_BLE);
}

base::string16 AuthenticatorBleEnterPairingModeSheetModel::GetStepTitle()
    const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_BLE_ENTER_PAIRING_MODE_TITLE);
}

base::string16 AuthenticatorBleEnterPairingModeSheetModel::GetStepDescription()
    const {
  return l10n_util::GetStringUTF16(
      IDS_WEBAUTHN_BLE_ENTER_PAIRING_MODE_DESCRIPTION);
}

// AuthenticatorBleDeviceSelectionSheetModel ----------------------------------

bool AuthenticatorBleDeviceSelectionSheetModel::IsActivityIndicatorVisible()
    const {
  return true;
}

gfx::ImageSkia* AuthenticatorBleDeviceSelectionSheetModel::GetStepIllustration()
    const {
  return GetImage(IDR_WEBAUTHN_ILLUSTRATION_BLE_NAME);
}

base::string16 AuthenticatorBleDeviceSelectionSheetModel::GetStepTitle() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_BLE_DEVICE_SELECTION_TITLE);
}

base::string16 AuthenticatorBleDeviceSelectionSheetModel::GetStepDescription()
    const {
  return l10n_util::GetStringUTF16(
      IDS_WEBAUTHN_BLE_DEVICE_SELECTION_DESCRIPTION);
}

// AuthenticatorBlePinEntrySheetModel -----------------------------------------

gfx::ImageSkia* AuthenticatorBlePinEntrySheetModel::GetStepIllustration()
    const {
  return GetImage(IDR_WEBAUTHN_ILLUSTRATION_BLE_PIN);
}

base::string16 AuthenticatorBlePinEntrySheetModel::GetStepTitle() const {
  return l10n_util::GetStringFUTF16(IDS_WEBAUTHN_BLE_PIN_ENTRY_TITLE,
                                    GetRelyingPartyIdString());
}

base::string16 AuthenticatorBlePinEntrySheetModel::GetStepDescription() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_BLE_PIN_ENTRY_DESCRIPTION);
}

bool AuthenticatorBlePinEntrySheetModel::IsAcceptButtonVisible() const {
  return true;
}

bool AuthenticatorBlePinEntrySheetModel::IsAcceptButtonEnabled() const {
  return true;
}

base::string16 AuthenticatorBlePinEntrySheetModel::GetAcceptButtonLabel()
    const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_BLE_PIN_ENTRY_NEXT);
}

// AuthenticatorBleVerifyingSheetModel ----------------------------------------

bool AuthenticatorBleVerifyingSheetModel::IsActivityIndicatorVisible() const {
  return true;
}

gfx::ImageSkia* AuthenticatorBleVerifyingSheetModel::GetStepIllustration()
    const {
  return GetImage(IDR_WEBAUTHN_ILLUSTRATION_BLE);
}

base::string16 AuthenticatorBleVerifyingSheetModel::GetStepTitle() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_BLE_VERIFYING_TITLE);
}

base::string16 AuthenticatorBleVerifyingSheetModel::GetStepDescription() const {
  return base::string16();
}

// AuthenticatorBleActivateSheetModel -----------------------------------------

AuthenticatorBleActivateSheetModel::AuthenticatorBleActivateSheetModel(
    AuthenticatorRequestDialogModel* dialog_model)
    : AuthenticatorSheetModelBase(dialog_model) {
  other_transports_menu_model_ = std::make_unique<OtherTransportsMenuModel>(
      dialog_model, AuthenticatorTransport::kBluetoothLowEnergy);
}

AuthenticatorBleActivateSheetModel::~AuthenticatorBleActivateSheetModel() =
    default;

bool AuthenticatorBleActivateSheetModel::IsActivityIndicatorVisible() const {
  return true;
}

gfx::ImageSkia* AuthenticatorBleActivateSheetModel::GetStepIllustration()
    const {
  return GetImage(IDR_WEBAUTHN_ILLUSTRATION_BLE_TAP);
}

base::string16 AuthenticatorBleActivateSheetModel::GetStepTitle() const {
  return l10n_util::GetStringFUTF16(IDS_WEBAUTHN_GENERIC_TITLE,
                                    GetRelyingPartyIdString());
}

base::string16 AuthenticatorBleActivateSheetModel::GetStepDescription() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_BLE_ACTIVATE_DESCRIPTION);
}

ui::MenuModel*
AuthenticatorBleActivateSheetModel::GetOtherTransportsMenuModel() {
  return other_transports_menu_model_.get();
}

// AuthenticatorTouchIdSheetModel -----------------------------------------

AuthenticatorTouchIdSheetModel::AuthenticatorTouchIdSheetModel(
    AuthenticatorRequestDialogModel* dialog_model)
    : AuthenticatorSheetModelBase(dialog_model) {
  other_transports_menu_model_ = std::make_unique<OtherTransportsMenuModel>(
      dialog_model, AuthenticatorTransport::kInternal);
}

AuthenticatorTouchIdSheetModel::~AuthenticatorTouchIdSheetModel() = default;

bool AuthenticatorTouchIdSheetModel::IsActivityIndicatorVisible() const {
  return true;
}

bool AuthenticatorTouchIdSheetModel::IsBackButtonVisible() const {
  // Clicking back would not dismiss the native Touch ID dialog, which would be
  // confusing. The user can cancel the native dialog to dismiss it.
  return false;
}

gfx::ImageSkia* AuthenticatorTouchIdSheetModel::GetStepIllustration() const {
#if defined(OS_MACOSX)
  return GetImage(IDR_WEBAUTHN_ILLUSTRATION_TOUCHID);
#else
  // Avoid bundling the PNG on platforms where it's not needed.
  return nullptr;
#endif  // defined(OS_MACOSX)
}

base::string16 AuthenticatorTouchIdSheetModel::GetStepTitle() const {
#if defined(OS_MACOSX)
  return l10n_util::GetStringFUTF16(IDS_WEBAUTHN_TOUCH_ID_TITLE,
                                    GetRelyingPartyIdString());
#else
  return base::string16();
#endif  // defined(OS_MACOSX)
}

base::string16 AuthenticatorTouchIdSheetModel::GetStepDescription() const {
  return base::string16();
}

ui::MenuModel* AuthenticatorTouchIdSheetModel::GetOtherTransportsMenuModel() {
  if (!other_transports_menu_model_) {
    other_transports_menu_model_ = std::make_unique<OtherTransportsMenuModel>(
        dialog_model(), AuthenticatorTransport::kInternal);
  }
  return other_transports_menu_model_.get();
}

// AuthenticatorPaaskSheetModel -----------------------------------------

AuthenticatorPaaskSheetModel::AuthenticatorPaaskSheetModel(
    AuthenticatorRequestDialogModel* dialog_model)
    : AuthenticatorSheetModelBase(dialog_model) {
  other_transports_menu_model_ = std::make_unique<OtherTransportsMenuModel>(
      dialog_model, AuthenticatorTransport::kCloudAssistedBluetoothLowEnergy);
}

AuthenticatorPaaskSheetModel::~AuthenticatorPaaskSheetModel() = default;

bool AuthenticatorPaaskSheetModel::IsActivityIndicatorVisible() const {
  return true;
}

gfx::ImageSkia* AuthenticatorPaaskSheetModel::GetStepIllustration() const {
  return GetImage(IDR_WEBAUTHN_ILLUSTRATION_PHONE);
}

base::string16 AuthenticatorPaaskSheetModel::GetStepTitle() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_CABLE_ACTIVATE_TITLE);
}

base::string16 AuthenticatorPaaskSheetModel::GetStepDescription() const {
  return l10n_util::GetStringUTF16(IDS_WEBAUTHN_CABLE_ACTIVATE_DESCRIPTION);
}

ui::MenuModel* AuthenticatorPaaskSheetModel::GetOtherTransportsMenuModel() {
  return other_transports_menu_model_.get();
}
