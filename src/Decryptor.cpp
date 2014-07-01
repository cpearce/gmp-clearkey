#include "stdafx.h"

using std::vector;

static Decryptor* instance = nullptr;

/* static */
Decryptor*
Decryptor::Get()
{
  return instance;
}

/* static */
void
Decryptor::Create(GMPDecryptorHost* aHost)
{
  assert(!Get());
  instance = new Decryptor(aHost);
}

Decryptor::Decryptor(GMPDecryptorHost* aHost)
  : mHost(aHost)
  , mNum(0)
  , mDecryptNumber(0)
{
  memset(&mEcount, 0, AES_BLOCK_SIZE);
}

void
Decryptor::Init(GMPDecryptorCallback* aCallback)
{
  mCallback = aCallback;
}

void
Decryptor::CreateSession(uint32_t aPromiseId,
                         const char* aInitDataType,
                         uint32_t aInitDataTypeSize,
                         const uint8_t* aInitData,
                         uint32_t aInitDataSize,
                         GMPSessionType aSessionType)
{
  static uint32_t sessionNum = 1;

  std::string sid = std::to_string(sessionNum++);
  mCallback->OnResolveNewSessionPromise(aPromiseId, sid.c_str(), sid.size());
  mCallback->OnSessionMessage(sid.c_str(), sid.size(),
                              aInitData, aInitDataSize,
                              "", 0);
}

void
Decryptor::LoadSession(uint32_t aPromiseId,
                       const char* aSessionId,
                       uint32_t aSessionIdLength)
{
}

void
Decryptor::UpdateSession(uint32_t aPromiseId,
                         const char* aSessionId,
                         uint32_t aSessionIdLength,
                         const uint8_t* aResponse,
                         uint32_t aResponseSize)
{
  eme_key_set keys;
  ExtractKeysFromJWKSet(std::string((const char*)aResponse, aResponseSize), keys);
  for (auto itr = keys.begin(); itr != keys.end(); itr++) {
    eme_key_id id = itr->first;
    eme_key key = itr->second;
    mKeySet[id] = key;
    mCallback->OnKeyIdUsable(aSessionId, aSessionIdLength, (uint8_t*)id.c_str(), id.length());
  }
}

void
Decryptor::CloseSession(uint32_t aPromiseId,
                        const char* aSessionId,
                        uint32_t aSessionIdLength)
{
  // Drop keys for session_id.
  // G. implementation:
  // https://code.google.com/p/chromium/codesearch#chromium/src/media/cdm/aes_decryptor.cc&sq=package:chromium&q=ReleaseSession&l=300&type=cs
  mKeySet.clear();
  //mHost->OnSessionClosed(session_id);
  //mActiveSessionId = ~0;
}

void
Decryptor::RemoveSession(uint32_t aPromiseId,
                         const char* aSessionId,
                         uint32_t aSessionIdLength)
{
}

void
Decryptor::SetServerCertificate(uint32_t aPromiseId,
                                const uint8_t* aServerCert,
                                uint32_t aServerCertSize)
{
}

bool
Decryptor::Decrypt(const uint8_t* aEncryptedBuffer,
                   const uint32_t aLength,
                   const GMPEncryptedBufferData* aCryptoData,
                   vector<uint8_t>& aOutDecrypted)
{
  std::string kid((const char*)aCryptoData->KeyId(), aCryptoData->KeyIdSize());
  if (mKeySet.find(kid) == mKeySet.end()) {
    // No key for that id.
    return false;
  }
  const std::string key = mKeySet[kid];
  if (AES_set_encrypt_key((uint8_t*)key.c_str(), 128, &mKey)) {
    LOG(L"Failed to set decryption key!\n");
    return false;
  }

  // Make one pass copying all encrypted data into a single buffer, then
  // another pass to decrypt it. Then another pass copying it into the
  // output buffer.

  vector<uint8_t> data;
  uint32_t index = 0;
  const uint32_t num_subsamples = aCryptoData->NumSubsamples();
  const uint32_t* clear_bytes = aCryptoData->ClearBytes();
  const uint32_t* cipher_bytes = aCryptoData->CipherBytes();
  for (uint32_t i=0; i<num_subsamples; i++) {
    index += clear_bytes[i];
    const uint8_t* start = aEncryptedBuffer + index;
    const uint8_t* end = start + cipher_bytes[i];
    data.insert(data.end(), start, end);
    index += cipher_bytes[i];
  }

  // Ensure the data length is a multiple of 16, and the extra data is
  // filled with 0's
  int32_t extra = (data.size() + AES_BLOCK_SIZE) % AES_BLOCK_SIZE;
  data.insert(data.end(), extra, uint8_t(0));

  // Decrypt the buffer.
  const uint32_t iterations = data.size() / AES_BLOCK_SIZE;
  vector<uint8_t> decrypted;
  decrypted.resize(data.size());
  uint8_t iv[AES_BLOCK_SIZE] = {0};
  memcpy(iv, aCryptoData->IV(), aCryptoData->IVSize());
  for (uint32_t i=0; i<iterations; i++) {
    uint8_t* in = data.data() + i*AES_BLOCK_SIZE;
    assert(in + AES_BLOCK_SIZE <= data.data() + data.size());
    uint8_t* out = decrypted.data() + i*AES_BLOCK_SIZE;
    assert(out + AES_BLOCK_SIZE <= decrypted.data() + decrypted.size());
    AES_ctr128_encrypt(in,
                       out,
                       AES_BLOCK_SIZE,
                       &mKey,
                       iv,
                       mEcount,
                       &mNum);
  }

  // Re-assemble the decrypted buffer.
  aOutDecrypted.clear();
  aOutDecrypted.resize(aLength);
  uint8_t* dst = aOutDecrypted.data();
  const uint8_t* src = aEncryptedBuffer;
  index = 0;
  uint32_t decrypted_index = 0;
  for (uint32_t i = 0; i < num_subsamples; i++) {
    memcpy(dst+index, src+index, clear_bytes[i]);
    index += clear_bytes[i];

    memcpy(dst+index, decrypted.data()+decrypted_index, cipher_bytes[i]);
    decrypted_index+= cipher_bytes[i];
    index += cipher_bytes[i];
    assert(index <= aLength);
    assert(decrypted_index <= aLength);
  }

  mDecryptNumber++;

  return true;
}
