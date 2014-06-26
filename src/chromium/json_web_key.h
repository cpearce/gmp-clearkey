// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_JSON_WEB_KEY_H_
#define MEDIA_CDM_JSON_WEB_KEY_H_

#include <string>
#include <utility>
#include <vector>
#include <stdint.h>
#include <array>
#include <map>

// A JSON Web Key Set looks like the following in JSON:
//   { "keys": [ JWK1, JWK2, ... ] }
// A symmetric keys JWK looks like the following in JSON:
//   { "kty":"oct",
//     "kid":"AQIDBAUGBwgJCgsMDQ4PEA",
//     "k":"FBUWFxgZGhscHR4fICEiIw" }
// There may be other properties specified, but they are ignored.
// Ref: http://tools.ietf.org/html/draft-ietf-jose-json-web-key and:
// http://tools.ietf.org/html/draft-jones-jose-json-private-and-symmetric-key
//
// For EME WD, both 'kid' and 'k' are base64 encoded strings, without trailing
// padding.

typedef std::string eme_key_id;
typedef std::string eme_key;
typedef std::map<eme_key_id, eme_key> eme_key_set;

// Checks if a key is null (all zeros).
// We use this to denote a kid that doesn't have a corresponding key.
inline bool IsNull(const eme_key& k) {
  const uint32_t* u = reinterpret_cast<const uint32_t*>(k.data());
  return *u == 0 &&
         *(u + 1) == 0 &&
         *(u + 2) == 0 &&
         *(u + 3) == 0;
}

// Extracts the JSON Web Keys from a JSON Web Key Set. If |input| looks like
// a valid JWK Set, then true is returned and |keys| is updated to contain
// the list of keys found. Otherwise return false.
bool ExtractKeysFromJWKSet(const std::string& jwk_set,
                           eme_key_set& keys);

#endif  // MEDIA_CDM_JSON_WEB_KEY_H_