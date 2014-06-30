/*
 * Copyright 2013, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

extern "C" {
#include <openssl/aes.h>
}

class Decryptor : public GMPDecryptor {
public:

  static Decryptor* Get();
  static void Create(GMPDecryptorHost* aHost);

  bool Decrypt(const uint8_t* aBuffer,
               const uint32_t aLength,
               const GMPEncryptedBufferData* aCryptoData,
               std::vector<uint8_t>& aOutDecrypted);

  // GMPDecryptor
  virtual void Init(GMPDecryptorCallback* aCallback) override;

  // Requests the creation of a session given |aType| and |aInitData|.
  // Decryptor should callback GMPDecryptorCallback::OnSessionCreated()
  // with the web session ID on success, or OnSessionError() on failure,
  // and then call OnSessionReady() once all keys for that session are
  // available.
  virtual void CreateSession(uint32_t aPromiseId,
                             const char* aInitDataType,
                             uint32_t aInitDataTypeSize,
                             const uint8_t* aInitData,
                             uint32_t aInitDataSize,
                             GMPSessionType aSessionType) override;

  virtual void LoadSession(uint32_t aPromiseId,
                           const char* aSessionId,
                           uint32_t aSessionIdLength) override;

  // Updates the session with |aResponse|.
  virtual void UpdateSession(uint32_t aPromiseId,
                             const char* aSessionId,
                             uint32_t aSessionIdLength,
                             const uint8_t* aResponse,
                             uint32_t aResponseSize) override;

  virtual void CloseSession(uint32_t aPromiseId,
                            const char* aSessionId,
                            uint32_t aSessionIdLength) override;

  virtual void RemoveSession(uint32_t aPromiseId,
                             const char* aSessionId,
                             uint32_t aSessionIdLength) override;

  // Resolve/reject promise on completion.
  virtual void SetServerCertificate(uint32_t aPromiseId,
                                    const uint8_t* aServerCert,
                                    uint32_t aServerCertSize) override;

private:

  Decryptor(GMPDecryptorHost* aHost);

  GMPDecryptorHost* mHost;
  GMPDecryptorCallback* mCallback;

  AES_KEY mKey;
  unsigned int mNum;
  unsigned char mEcount[AES_BLOCK_SIZE];
  uint32_t mDecryptNumber;
};
