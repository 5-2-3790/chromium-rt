// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_NIGORI_H_
#define COMPONENTS_SYNC_BASE_NIGORI_H_

#include <stddef.h>

#include <memory>
#include <string>

#include "components/sync/base/passphrase_enums.h"

namespace crypto {
class SymmetricKey;
}  // namespace crypto

namespace syncer {

// A (partial) implementation of Nigori, a protocol to securely store secrets in
// the cloud. This implementation does not support server authentication or
// assisted key derivation.
//
// To store secrets securely, use the |Permute| method to derive a lookup name
// for your secret (basically a map key), and |Encrypt| and |Decrypt| to store
// and retrieve the secret.
//
// https://www.cl.cam.ac.uk/~drt24/nigori/nigori-overview.pdf
class Nigori {
 public:
  enum Type {
    Password = 1,
  };

  Nigori();
  virtual ~Nigori();

  // Initialize by deriving keys based on the given |method|, |hostname|,
  // |username| and |password|.
  bool InitByDerivation(KeyDerivationMethod method,
                        const std::string& hostname,
                        const std::string& username,
                        const std::string& password);

  // Initialize by importing the given keys instead of deriving new ones.
  bool InitByImport(const std::string& user_key,
                    const std::string& encryption_key,
                    const std::string& mac_key);

  // Derives a secure lookup name from |type| and |name|. If |hostname|,
  // |username| and |password| are kept constant, a given |type| and |name| pair
  // always yields the same |permuted| value. Note that |permuted| will be
  // Base64 encoded.
  bool Permute(Type type, const std::string& name, std::string* permuted) const;

  // Encrypts |value|. Note that on success, |encrypted| will be Base64
  // encoded.
  bool Encrypt(const std::string& value, std::string* encrypted) const;

  // Decrypts |value| into |decrypted|. It is assumed that |value| is Base64
  // encoded.
  bool Decrypt(const std::string& value, std::string* decrypted) const;

  // Exports the raw derived keys.
  void ExportKeys(std::string* user_key,
                  std::string* encryption_key,
                  std::string* mac_key) const;

  // Exposed for tests.
  static const size_t kIvSize = 16;

 private:
  struct Keys {
    Keys();
    ~Keys();

    // TODO(vitaliii): user_key isn't used any more, but legacy clients will
    // fail to import a nigori node without one. We preserve it for the sake of
    // those clients, but it should be removed once enough clients have upgraded
    // to code that doesn't enforce its presence.
    std::unique_ptr<crypto::SymmetricKey> user_key;
    std::unique_ptr<crypto::SymmetricKey> encryption_key;
    std::unique_ptr<crypto::SymmetricKey> mac_key;

    bool InitByDerivationUsingPbkdf2(const std::string& hostname,
                                     const std::string& username,
                                     const std::string& password);
    bool InitByDerivationUsingScrypt(const std::string& password);
    bool InitByImport(const std::string& user_key_str,
                      const std::string& encryption_key_str,
                      const std::string& mac_key_str);
  };

  Keys keys_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_NIGORI_H_
