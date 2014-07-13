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

#include "stdafx.h"

class WriteRecordClient : public GMPRecordClient {
public:
  GMPErr Init(GMPRecord* aRecord,
              GMPTask* aContinuation,
              const uint8_t* aData,
              uint32_t aDataSize) {
    mRecord = aRecord;
    mContinuation = aContinuation;
    mData.insert(mData.end(), aData, aData + aDataSize);
    return mRecord->Open();
  }

  virtual void OnOpenComplete(GMPErr aStatus) override {
    mRecord->Write(mData.data(), mData.size());
  }

  virtual void OnReadComplete(GMPErr aStatus,
                              const uint8_t* aData,
                              uint32_t aDataSize) override {}

  virtual void OnWriteComplete(GMPErr aStatus) override {
    // Note: Call Close() before running continuation, in case the
    // continuation tries to open the same record; if we call Close()
    // after running the continuation, the Close() call will arrive
    // just after the Open() call succeeds, immediately closing the
    // record we just opened.
    mRecord->Close();
    if (mContinuation) {
      GMPRunOnMainThread(mContinuation);
    }
    delete this;
  }

private:
  GMPRecord* mRecord;
  GMPTask* mContinuation;
  std::vector<uint8_t> mData;
};

GMPErr
WriteRecord(const std::string& aRecordName,
            const uint8_t* aData,
            uint32_t aNumBytes,
            GMPTask* aContinuation)
{
  GMPRecord* record;
  WriteRecordClient* client = new WriteRecordClient();
  auto err = GMPOpenRecord(aRecordName.c_str(),
                           aRecordName.size(),
                           &record,
                           client);
  if (GMP_FAILED(err)) {
    return err;
  }
  return client->Init(record, aContinuation, aData, aNumBytes);
}

GMPErr
WriteRecord(const std::string& aRecordName,
            const std::string& aData,
            GMPTask* aContinuation)
{
  return WriteRecord(aRecordName,
                     (const uint8_t*)aData.c_str(),
                     aData.size(),
                     aContinuation);
}


class ReadRecordClient : public GMPRecordClient {
public:
  GMPErr Init(GMPRecord* aRecord,
              ReadContinuation* aContinuation) {
    mRecord = aRecord;
    mContinuation = aContinuation;
    return mRecord->Open();
  }

  virtual void OnOpenComplete(GMPErr aStatus) override {
    auto err = mRecord->Read();
    if (GMP_FAILED(err)) {
      mContinuation->OnReadComplete(err, "");
      delete this;
    }
  }

  virtual void OnReadComplete(GMPErr aStatus,
                              const uint8_t* aData,
                              uint32_t aDataSize) override {
    // Note: Call Close() before running continuation, in case the
    // continuation tries to open the same record; if we call Close()
    // after running the continuation, the Close() call will arrive
    // just after the Open() call succeeds, immediately closing the
    // record we just opened.
    mRecord->Close();
    std::string data((const char*)aData, aDataSize);
    mContinuation->OnReadComplete(GMPNoErr, data);
    delete this;
  }

  virtual void OnWriteComplete(GMPErr aStatus) override {
  }

private:
  GMPRecord* mRecord;
  ReadContinuation* mContinuation;
};

GMPErr
ReadRecord(const std::string& aRecordName,
           ReadContinuation* aContinuation)
{
  assert(aContinuation);
  GMPRecord* record;
  ReadRecordClient* client = new ReadRecordClient();
  auto err = GMPOpenRecord(aRecordName.c_str(),
                           aRecordName.size(),
                           &record,
                           client);
  if (GMP_FAILED(err)) {
    return err;
  }
  return client->Init(record, aContinuation);
}
