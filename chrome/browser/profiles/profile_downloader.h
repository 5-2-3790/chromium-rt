// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_DOWNLOADER_H_
#define CHROME_BROWSER_PROFILES_PROFILE_DOWNLOADER_H_

#include <memory>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "base/strings/string16.h"
#include "chrome/browser/image_decoder.h"
#include "components/signin/core/browser/account_info.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace identity {
class PrimaryAccountAccessTokenFetcher;
}
class ProfileDownloaderDelegate;

// Downloads user profile information. The profile picture is decoded in a
// sandboxed process.
class ProfileDownloader : public ImageDecoder::ImageRequest,
                          public identity::IdentityManager::Observer,
                          public AccountTrackerService::Observer {
 public:
  enum PictureStatus {
    PICTURE_SUCCESS,
    PICTURE_FAILED,
    PICTURE_DEFAULT,
    PICTURE_CACHED,
  };

  explicit ProfileDownloader(ProfileDownloaderDelegate* delegate);
  ~ProfileDownloader() override;

  // Starts downloading profile information if the necessary authorization token
  // is ready. If not, subscribes to token service and starts fetching if the
  // token is available. Should not be called more than once.
  virtual void Start();

  // Starts downloading profile information if the necessary authorization token
  // is ready. If not, subscribes to token service and starts fetching if the
  // token is available. Should not be called more than once.
  virtual void StartForAccount(const std::string& account_id);

  // On successful download this returns the hosted domain of the user.
  virtual base::string16 GetProfileHostedDomain() const;

  // On successful download this returns the full name of the user. For example
  // "Pat Smith".
  virtual base::string16 GetProfileFullName() const;

  // On successful download this returns the given name of the user. For example
  // if the name is "Pat Smith", the given name is "Pat".
  virtual base::string16 GetProfileGivenName() const;

  // On successful download this returns G+ locale preference of the user.
  virtual std::string GetProfileLocale() const;

  // On successful download this returns the profile picture of the user.
  // For users with no profile picture set (that is, they have the default
  // profile picture) this will return an Null bitmap.
  virtual SkBitmap GetProfilePicture() const;

  // Gets the profile picture status.
  virtual PictureStatus GetProfilePictureStatus() const;

  // Gets the URL for the profile picture. This can be cached so that the same
  // picture is not downloaded multiple times. This value should only be used
  // when the picture status is PICTURE_SUCCESS.
  virtual std::string GetProfilePictureURL() const;

 private:
  friend class ProfileDownloaderTest;
  FRIEND_TEST_ALL_PREFIXES(ProfileDownloaderTest, AccountInfoReady);
  FRIEND_TEST_ALL_PREFIXES(ProfileDownloaderTest, AccountInfoNotReady);
  FRIEND_TEST_ALL_PREFIXES(ProfileDownloaderTest,
                           AccountInfoNoPictureDoesNotCrash);
  FRIEND_TEST_ALL_PREFIXES(ProfileDownloaderTest,
                           AccountInfoInvalidPictureURLDoesNotCrash);

  void FetchImageData();

  void OnURLLoaderComplete(std::unique_ptr<std::string> response_body);

  // Overriden from ImageDecoder::ImageRequest:
  void OnImageDecoded(const SkBitmap& decoded_image) override;
  void OnDecodeImageFailed() override;

  // Overriden from identity::IdentityManager::Observer:
  void OnRefreshTokenUpdatedForAccount(const AccountInfo& account_info,
                                       bool is_valid) override;

  // Implementation of AccountTrackerService::Observer.
  void OnAccountUpdated(const AccountInfo& info) override;

  // Callback for PrimaryAccountAccessTokenFetcher.
  void OnAccessTokenFetchComplete(GoogleServiceAuthError error,
                                  identity::AccessTokenInfo access_token_info);

  // Issues the first request to get user profile image.
  void StartFetchingImage();

  // Gets the authorization header.
  const char* GetAuthorizationHeader() const;

  // Starts fetching OAuth2 access token. This is needed before the GAIA info
  // can be downloaded.
  void StartFetchingOAuth2AccessToken();

  ProfileDownloaderDelegate* delegate_;
  std::string account_id_;
  std::string auth_token_;
  std::unique_ptr<network::SimpleURLLoader> simple_loader_;
  std::unique_ptr<identity::PrimaryAccountAccessTokenFetcher>
      oauth2_access_token_fetcher_;
  AccountInfo account_info_;
  SkBitmap profile_picture_;
  PictureStatus picture_status_;
  AccountTrackerService* account_tracker_service_;
  identity::IdentityManager* identity_manager_;
  ScopedObserver<identity::IdentityManager, identity::IdentityManager::Observer>
      identity_manager_observer_;
  bool waiting_for_account_info_;

  DISALLOW_COPY_AND_ASSIGN(ProfileDownloader);
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_DOWNLOADER_H_
