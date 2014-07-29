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
               const GMPEncryptedBufferMetadata* aCryptoData,
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

  // Releases the resources (keys) for the specified session.
  virtual void CloseSession(uint32_t aPromiseId,
                            const char* aSessionId,
                            uint32_t aSessionIdLength) override;

  // Removes the resources (keys) for the specified session.
  virtual void RemoveSession(uint32_t aPromiseId,
                             const char* aSessionId,
                             uint32_t aSessionIdLength) override;

  // Resolve/reject promise on completion.
  virtual void SetServerCertificate(uint32_t aPromiseId,
                                    const uint8_t* aServerCert,
                                    uint32_t aServerCertSize) override;

  virtual void Decrypt(GMPBuffer* aBuffer,
                       GMPEncryptedBufferMetadata* aMetadata) override;

  virtual void DecryptingComplete() override;

#ifdef TEST_GMP_STORAGE
  void SessionIdReady(uint32_t aPromiseId,
                    uint32_t aSessionId,
                    const std::vector<uint8_t>& aInitData);
#endif

  void SendSessionMessage(const std::string& aSessionId,
                          const std::string& aMessage);
private:
  GMPDecryptorCallback* mCallback;

  class MessageTask : public GMPTask {
  public:
    MessageTask(GMPDecryptorCallback* aCallback,
                const std::string& aSessionId,
                const std::string& aMessage)
      : mCallback(aCallback)
      , mSessionId(aSessionId)
      , mMessage(aMessage)
    {
    }
    virtual void Destroy() override {
      delete this;
    }
    virtual void Run() override {
      mCallback->SessionMessage(mSessionId.c_str(), mSessionId.size(),
                                (const uint8_t*)mMessage.c_str(), mMessage.size(),
                                "", 0);
    }
    GMPDecryptorCallback* mCallback;
    std::string mSessionId;
    std::string mMessage;
  };

  Decryptor(GMPDecryptorHost* aHost);

  GMPDecryptorHost* mHost;

  AES_KEY mKey;
  unsigned int mNum;
  unsigned char mEcount[AES_BLOCK_SIZE];
  uint32_t mDecryptNumber;
  eme_key_set mKeySet;
};
