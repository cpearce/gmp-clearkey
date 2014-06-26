// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This code was originally tkane from Chromium, but I used a different
// JSON parser here so that I didn't need to pull in half of Chromium
// in order to build the code here, so I ended up rewriting most of this
// code. -cpearce.

#include "json_web_key.h"
#include "json/reader.h"
#include "modp_b64/modp_b64.h"

#include <memory>
#include <iostream>
#include <assert.h>
#include <string>

extern void LOG(wchar_t* format, ...);

const char kBase64Padding = '=';

// Decodes an unpadded base64 string. Returns empty string on error.
bool DecodeBase64(const std::string& encoded_text,
                  std::string& decoded_text) {
  // TOOD_cpearce: I couldn't find this constraint mentioned in
  //               the EME spec anywhere.
  //// EME spec doesn't allow padding characters.
  //if (encoded_text.find_first_of(kBase64Padding) != std::string::npos)
  //  return false;

  // Since base::Base64Decode() requires padding characters, add them so length
  // of |encoded_text| is exactly a multiple of 4.
  size_t num_last_grouping_chars = encoded_text.length() % 4;
  std::string modified_text = encoded_text;
  if (num_last_grouping_chars > 0) {
    modified_text.append(4 - num_last_grouping_chars, kBase64Padding);
  }

  decoded_text.resize(modp_b64_decode_len(modified_text.size()));
  size_t output_size = modp_b64_decode(&(decoded_text[0]),
                                       &(modified_text[0]),
                                       modified_text.size());
  if (output_size == MODP_B64_ERROR) {
    return false;
  }
  decoded_text.resize(output_size); // strips off null byte

  return true;
}

// From chromium /base/strings/string_util.h
// Hack to convert any char-like type to its unsigned counterpart.
// For example, it will convert char, signed char and unsigned char to unsigned
// char.
template<typename T>
struct ToUnsigned {
  typedef T Unsigned;
};

template<>
struct ToUnsigned<char> {
  typedef unsigned char Unsigned;
};
template<>
struct ToUnsigned<signed char> {
  typedef unsigned char Unsigned;
};
template<>
struct ToUnsigned<wchar_t> {
#if defined(WCHAR_T_IS_UTF16)
  typedef unsigned short Unsigned;
#elif defined(WCHAR_T_IS_UTF32)
  typedef uint32 Unsigned;
#endif
};
template<>
struct ToUnsigned<short> {
  typedef unsigned short Unsigned;
};

// From base/strings/string_util.cc
template<class STR>
static bool IsStringASCII(const STR& str) {
  for (size_t i = 0; i < str.length(); i++) {
    typename ToUnsigned<typename STR::value_type>::Unsigned c = str[i];
    if (c > 0x7F)
      return false;
  }
  return true;
}

static bool
AddEmeKeysToSet(const Json::Value& object, eme_key_set& keyvals)
{
  assert(object.type() == Json::objectValue);

  Json::Value val;
  
  // Check that the key type is correct.
  val = object["kty"];
  if (val.type() != Json::stringValue ||
      val.asString() != "oct") {
    return false;
  }

  // Check that we have a key id.
  val = object["kid"];
  if (val.type() != Json::stringValue) {
    return false;
  }
  eme_key_id kid;
  std::string encoded = val.asString();
  std::string decoded;
  if (!DecodeBase64(encoded, kid) || kid.size() != 16) {
    LOG(L"Key_id had invalid base64 encoding.\n");
    return false;
  }

  val = object["k"];
  eme_key k;
  if (val.type() != Json::stringValue) {
    return false;
  }
  encoded = val.asString();
  if (!DecodeBase64(encoded, k) || k.size() != 16) {
    LOG(L"Key had invalid base64 encoding.\n");
    return false;
  }

  keyvals[kid] = k;

  return true;
}

inline std::wstring widen(const std::string &narrow) {
  std::wstring ws(narrow.begin(), narrow.end());
  return ws;
}

bool ExtractKeysFromJWKSet(const std::string& jwk_set, eme_key_set& keys) {
  if (!IsStringASCII(jwk_set)) {
    return false;
  }

  LOG(L"Parsing key %s\n", widen(jwk_set).c_str());

  Json::Reader reader;
  Json::Value root;
  bool ok = reader.parse(jwk_set.data(), jwk_set.data() + jwk_set.size(), root);
  
  if (!ok) {
    std::string msg = reader.getFormatedErrorMessages();
    LOG(L"JSON parsing failed: %s", widen(msg).c_str());
    return false;
  }
  
  if (root.type() != Json::objectValue) {
    return false;
  }

  // Find the "keys" object/dictionary in the JSON data.
  Json::Value keysArray = root.get("keys", Json::nullValue);
  if (keysArray.type() != Json::arrayValue) {
    return false;
  }
  
  // Iterate over the array of keys, extracting key->value pairs.
  for (uint32_t i = 0; i < keysArray.size(); i++) {
    Json::Value key = keysArray[i];
    if (key.type() == Json::objectValue) {
      AddEmeKeysToSet(key, keys);
    }
  }

  return true;
}
