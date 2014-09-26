#include "stdafx.h"

#include <algorithm>

using std::vector;

static Decryptor* instance = nullptr;

Decryptor::Decryptor(GMPDecryptorHost* aHost)
  : mCallback(nullptr)
  , mHost(aHost)
  , mDecryptNumber(0)
{
}

static std::vector<Decryptor*> sDecryptors;

/* static */
Decryptor*
Decryptor::Get(const GMPEncryptedBufferMetadata* aCryptoData)
{
  // We can have multiple GMPDecryptors active in the same plugin instance,
  // as the plugin is shared between all media elements in the same origin.
  // So we maintain a list of the active GMPDecryptors, and select the newest
  // one that can decrypt the buffer. This may not be correct if there is
  // multiple content sharing the same key in the same origin...
  const std::string kid((const char*)aCryptoData->KeyId(), aCryptoData->KeyIdSize());

  auto itr = find_if(sDecryptors.rbegin(),
                     sDecryptors.rend(),
                     [&](Decryptor* d)->bool { return d->CanDecryptKey(kid); });
  return (itr != sDecryptors.rend()) ? *itr : nullptr;
}

bool
Decryptor::CanDecryptKey(const std::string& aKeyId)
{
  return mCallback && mKeySet.find(aKeyId) != mKeySet.end();
}

void
Decryptor::Init(GMPDecryptorCallback* aCallback)
{
  mCallback = aCallback;
  sDecryptors.push_back(this);
}

#ifdef TEST_GMP_STORAGE
static const std::string SessionIdRecordName = "sessionid";

class ReadShutdownTimeTask : public ReadContinuation {
public:
  ReadShutdownTimeTask(Decryptor* aDecryptor,
                       const std::string& aSessionId)
    : mDecryptor(aDecryptor)
    , mSessionId(aSessionId)
  {
  }
  void ReadComplete(GMPErr aErr, const std::string& aData) override {
    if (!aData.size()) {
      mDecryptor->SendSessionMessage(mSessionId, "First run");
    } else {
      GMPTimestamp s = _atoi64(aData.c_str());
      GMPTimestamp t = 0;
      GMPGetCurrentTime(&t);
      t -= s;
      std::string msg = "ClearKey CDM last shutdown " + std::to_string(t/ 1000) + "s ago.";
      mDecryptor->SendSessionMessage(mSessionId, msg);
    }
  }
  Decryptor* mDecryptor;
  std::string mSessionId;
};
#endif

void
Decryptor::SendSessionMessage(const std::string& aSessionId,
                              const std::string& aMessage)
{
  mCallback->SessionMessage(aSessionId.c_str(),
                            aSessionId.size(),
                            (const uint8_t*)aMessage.c_str(),
                            aMessage.size(),
                            nullptr,
                            0);
}

#ifdef TEST_GMP_STORAGE
void
Decryptor::SessionIdReady(uint32_t aPromiseId,
                          uint32_t aSessionId,
                          const std::vector<uint8_t>& aInitData)
{

  std::string sid = std::to_string(aSessionId);
  mCallback->ResolveNewSessionPromise(aPromiseId, sid.c_str(), sid.size());
  mCallback->SessionMessage(sid.c_str(), sid.size(),
                            aInitData.data(), aInitData.size(),
                            "", 0);

  uint32_t nextId = aSessionId + 1;
  WriteRecord(SessionIdRecordName, std::to_string(nextId), nullptr);

#ifdef TEST_GMP_TIMER
  SendMessageToNotifyOfSessionId(sid);
#endif // TEST_GMP_TIMER

#if defined(TEST_GMP_ASYNC_SHUTDOWN)
  ReadRecord(SHUTDOWN_TIME_RECORD, new ReadShutdownTimeTask(this, sid));
#endif

#ifdef TEST_NODE_ID
  const char* _id = nullptr;
  uint32_t len = 0;
  mHost->GetNodeId(&_id, &len);
  std::string id(_id, len);
  std::string msg = "Node ID is: " + id;
  mCallback->SessionMessage(sid.c_str(), sid.size(),
                            (uint8_t*)msg.c_str(), msg.size(),
                            "", 0);
#endif
}

class ReadSessionId : public ReadContinuation {
public:
  ReadSessionId(Decryptor* aDecryptor,
                uint32_t aPromiseId,
                const std::string& aInitDataType,
                vector<uint8_t>& aInitData,
                GMPSessionType aSessionType)
    : mDecryptor(aDecryptor)
    , mPromiseId(aPromiseId)
    , mInitDataType(aInitDataType)
    , mInitData(std::move(aInitData))
  {
  }

  void ReadComplete(GMPErr aErr, const std::string& aData) override {
    uint32_t sid = atoi(aData.c_str());
    mDecryptor->SessionIdReady(mPromiseId,
                               sid,
                               mInitData);
    delete this;
  }

  std::string mSessionId;

private:
  Decryptor* mDecryptor;
  uint32_t mPromiseId;
  std::string mInitDataType;
  vector<uint8_t> mInitData;
};

#define TRUNCATE_RECORD "truncate-record"
#define TRUNCATE_DATA "I will soon be truncated"

class ReadThenTask : public GMPTask {
public:
  ReadThenTask(ReadContinuation* aThen)
    : mThen(aThen)
  {}
  void Run() override {
    ReadRecord(TRUNCATE_RECORD, mThen);
  }
  void Destroy() override {
    delete this;
  }
  ReadContinuation* mThen;
};

class TestEmptyContinuation : public ReadContinuation {
public:
  void ReadComplete(GMPErr aErr, const std::string& aData) override {
    assert(aData == "");
    delete this;
  }
};

class TruncateContinuation : public ReadContinuation {
public:
  void ReadComplete(GMPErr aErr, const std::string& aData) override {
    assert(aData == TRUNCATE_DATA);
    WriteRecord(TRUNCATE_RECORD, nullptr, 0, new ReadThenTask(new TestEmptyContinuation()));
    delete this;
  }
};

#endif // TEST_GMP_STORAGE

#ifdef TEST_GMP_TIMER

class AssertFalseTask : public GMPTask {
public:
  void Run() override {
    assert(false);
  }
  void Destroy() override {
    delete this;
  }
};

class SetOffMainThreadTimerTask : public GMPTask {
public:
  SetOffMainThreadTimerTask(GMPThread* aThread)
    : mThread(aThread)
  {
  }

  void Run() override {
    // Assert that setting a null timer fails.
    assert(GMP_FAILED(GMPSetTimer(nullptr, 1000)));

    auto t = new AssertFalseTask();
    auto err = GMPSetTimer(t, 1000);
    assert(GMP_FAILED(err));
    if (GMP_FAILED(err)) {
      delete t;
    }
    // Shutdown the thread.
    GMPRunOnMainThread(WrapTask(mThread, &GMPThread::Join));
  }
  void Destroy() override {
    delete this;
  }
private:
  GMPThread* mThread;
};

void
Decryptor::SendMessageToNotifyOfSessionId(const std::string& aSid)
{
  std::string msg = "Message for you sir! (Sent by a timer)";
  GMPTimestamp t = 0;
  auto err = GMPGetCurrentTime(&t);
  if (GMP_SUCCEEDED(err)) {
    msg += " time=" + std::to_string(t);
  }
  GMPTask* task = new MessageTask(mCallback, aSid, msg);
  GMPSetTimer(task, 3000);
}
#endif

void
Decryptor::CreateSession(uint32_t aPromiseId,
                         const char* aInitDataType,
                         uint32_t aInitDataTypeSize,
                         const uint8_t* aInitData,
                         uint32_t aInitDataSize,
                         GMPSessionType aSessionType)
{
  GMPErr err;
#ifdef TEST_GMP_STORAGE
  const std::string initDataType(aInitDataType, aInitDataTypeSize);
  vector<uint8_t> initData;
  initData.insert(initData.end(), aInitData, aInitData+aInitDataSize);
  err = ReadRecord(SessionIdRecordName,
                   new ReadSessionId(this,
                                     aPromiseId,
                                     initDataType,
                                     initData,
                                     aSessionType));
  if (GMP_FAILED(err)) {
    std::string msg = "ClearKeyGMP: Failed to read from storage.";
    mCallback->SessionError(nullptr, 0,
                            kGMPInvalidStateError,
                            42,
                            msg.c_str(),
                            msg.size());
  }
#ifdef TEST_GMP_STORAGE_TRUNCATE
  WriteRecord(TRUNCATE_RECORD,
              TRUNCATE_DATA,
              new ReadThenTask(new TruncateContinuation()));
#endif
#else
  static uint32_t gSessionCount = 1;
  std::string sid = std::to_string(gSessionCount++);
  mCallback->ResolveNewSessionPromise(aPromiseId, sid.c_str(), sid.size());
  mCallback->SessionMessage(sid.c_str(), sid.size(),
                            aInitData, aInitDataSize,
                            "", 0);
#ifdef TEST_GMP_TIMER
  SendMessageToNotifyOfSessionId(sid);
#endif // TEST_GMP_TIMER

#ifdef TEST_NODE_ID
  const char* _id = nullptr;
  uint32_t len = 0;
  mHost->GetNodeId(&_id, &len);
  std::string id(_id, len);
  std::string msg = "Node ID is: " + id;
  mCallback->SessionMessage(sid.c_str(), sid.size(),
                            (uint8_t*)msg.c_str(), msg.size(),
                            "", 0);
#endif


#endif // !TEST_GMP_STORAGE


#ifdef TEST_GMP_TIMER
  // Assert that setting a timer on a non-main thread fails.
  GMPThread* thread = nullptr;
  err = GMPCreateThread(&thread);
  assert(GMP_SUCCEEDED(err));
  if (GMP_SUCCEEDED(err)) {
    thread->Post(new SetOffMainThreadTimerTask(thread));
  }
#endif
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
  // Notify Gecko of all the keys that are ready to decode with. Gecko
  // will only try to decrypt data for keyIds which the GMP/CDM has said
  // are usable. So don't forget to do this, otherwise the CDM won't be
  // asked to decode/decrypt anything!
  eme_key_set keys;
  ExtractKeysFromJWKSet(std::string((const char*)aResponse, aResponseSize), keys);
  for (auto itr = keys.begin(); itr != keys.end(); itr++) {
    eme_key_id id = itr->first;
    eme_key key = itr->second;
    mKeySet[id] = key;
    mCallback->KeyIdUsable(aSessionId,
                             aSessionIdLength,
                             (uint8_t*)id.c_str(),
                             id.length());
  }

#ifdef TEST_DECODING
  mCallback->SetCapabilities(GMP_EME_CAP_DECRYPT_AND_DECODE_AUDIO |
                             GMP_EME_CAP_DECRYPT_AND_DECODE_VIDEO);
#else
  mCallback->SetCapabilities(GMP_EME_CAP_DECRYPT_AUDIO |
                             GMP_EME_CAP_DECRYPT_VIDEO);
#endif

  std::string msg = "ClearKeyGMP says UpdateSession is throwing fake error/exception";
  mCallback->SessionError(aSessionId, aSessionIdLength,
                          kGMPInvalidStateError,
                          42,
                          msg.c_str(),
                          msg.size());
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

void
Decryptor::Decrypt(GMPBuffer* aBuffer,
                   GMPEncryptedBufferMetadata* aMetadata)
{
  vector<uint8_t> decrypted;
  if (!Decrypt(aBuffer->Data(),
               aBuffer->Size(),
               aMetadata,
               decrypted)) {
    mCallback->Decrypted(aBuffer, GMPCryptoErr);
  } else {
    memcpy(aBuffer->Data(), decrypted.data(), aBuffer->Size());
    mCallback->Decrypted(aBuffer, GMPNoErr);
  }
}

bool
Decryptor::Decrypt(const uint8_t* aEncryptedBuffer,
                   const uint32_t aLength,
                   const GMPEncryptedBufferMetadata* aCryptoData,
                   vector<uint8_t>& aOutDecrypted)
{
  std::string kid((const char*)aCryptoData->KeyId(), aCryptoData->KeyIdSize());
  if (mKeySet.find(kid) == mKeySet.end()) {
    // No key for that id.
    return false;
  }
  const std::string key = mKeySet[kid];

  AES_KEY aesKey;
  if (AES_set_encrypt_key((uint8_t*)key.c_str(), 128, &aesKey)) {
    LOG(L"Failed to set decryption key!\n");
    return false;
  }

  // Make one pass copying all encrypted data into a single buffer, then
  // another pass to decrypt it. Then another pass copying it into the
  // output buffer.

  vector<uint8_t> data;
  uint32_t index = 0;
  const uint32_t num_subsamples = aCryptoData->NumSubsamples();
  const uint16_t* clear_bytes = aCryptoData->ClearBytes();
  const uint32_t* cipher_bytes = aCryptoData->CipherBytes();
  for (uint32_t i=0; i<num_subsamples; i++) {
    index += clear_bytes[i];
    const uint8_t* start = aEncryptedBuffer + index;
    const uint8_t* end = start + cipher_bytes[i];
    data.insert(data.end(), start, end);
    index += cipher_bytes[i];
  }

  // Decrypt the buffer.
  const uint32_t iterations = data.size() / AES_BLOCK_SIZE;
  vector<uint8_t> decrypted;
  decrypted.resize(data.size());

  uint32_t num = 0;
  uint8_t ecount[AES_BLOCK_SIZE] = {0};
  uint8_t iv[AES_BLOCK_SIZE] = {0};
  memcpy(iv, aCryptoData->IV(), aCryptoData->IVSize());

  AES_ctr128_encrypt(data.data(),
                     decrypted.data(),
                     data.size(),
                     &aesKey,
                     iv,
                     ecount,
                     &num);

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

void
Decryptor::DecryptingComplete()
{
  mCallback = nullptr;
  sDecryptors.erase(find(sDecryptors.begin(), sDecryptors.end(), this));
}
