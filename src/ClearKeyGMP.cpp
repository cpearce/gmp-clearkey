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

#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "Winmm.lib")
#pragma comment(lib, "wmcodecdspuuid")
#pragma comment(lib, "mfplat.lib")

static GMPPlatformAPI* sPlatformAPI = nullptr;

GMPErr
GMPCreateThread(GMPThread** aThread)
{
  assert(sPlatformAPI);
  return sPlatformAPI->createthread(aThread);
}

GMPErr
GMPRunOnMainThread(GMPTask* aTask)
{
  assert(sPlatformAPI);
  return sPlatformAPI->runonmainthread(aTask);
}

GMPErr
GMPSyncRunOnMainThread(GMPTask* aTask)
{
  assert(sPlatformAPI);
  return sPlatformAPI->syncrunonmainthread(aTask);
}

GMPErr
GMPCreateMutex(GMPMutex** aMutex)
{
  assert(sPlatformAPI);
  return sPlatformAPI->createmutex(aMutex);
}

GMPErr
GMPOpenRecord(const char* aName,
              uint32_t aNameLength,
              GMPRecord** aOutRecord,
              GMPRecordClient* aClient)
{
  return sPlatformAPI->createrecord(aName, aNameLength, aOutRecord, aClient);
}

GMPErr
GMPSetTimer(GMPTask* aTask, int64_t aTimeoutMS)
{
  return sPlatformAPI->settimer(aTask, aTimeoutMS);
}

GMPErr
GMPGetCurrentTime(GMPTimestamp* aOutTime)
{
  return sPlatformAPI->getcurrenttime(aOutTime);
}

class FinishShutdownTask : public GMPTask {
public:
  FinishShutdownTask(GMPAsyncShutdownHost* aHost)
    : mHost(aHost)
  {}

  void Run() override {
    mHost->ShutdownComplete();
  }

  void Destroy() override {
    delete this;
  }
private:
  GMPAsyncShutdownHost* mHost;
};

class WriteTimeClient : public GMPRecordClient {
public:
  void Init(GMPRecord* aRecord, GMPTask* aContinuation) {
    mRecord = aRecord;
    mContinuation = aContinuation;
    auto err = mRecord->Open();
    assert(GMP_SUCCEEDED(err));
  }

  virtual void OnOpenComplete(GMPErr aStatus) override {
    GMPTimestamp t;
    auto err = GMPGetCurrentTime(&t);
    std::string msg;
    if (GMP_SUCCEEDED(err)) {
      msg = "Shutdowm time: " + std::to_string(t);
    } else {
      msg = "Unable to retrieve shutdown time";
    }
    mRecord->Write((const uint8_t*)msg.c_str(), msg.size());
  }

  virtual void OnReadComplete(GMPErr aStatus,
                              const uint8_t* aData,
                              uint32_t aDataSize) override {}

  virtual void OnWriteComplete(GMPErr aStatus) override {
    GMPRunOnMainThread(mContinuation);
  }

private:
  GMPRecord* mRecord;
  GMPTask* mContinuation;
};


class AsyncShutdown : public GMPAsyncShutdown {
public:
  AsyncShutdown(GMPAsyncShutdownHost* aHost)
    : mHost(aHost)
  {}

  void BeginShutdown() {
    const char* name = "shutdown-time";
    GMPRecord* record;
    WriteTimeClient* client = new WriteTimeClient();
    auto err = GMPOpenRecord(name, strlen(name), &record, client);
    if (GMP_SUCCEEDED(err)) {
      client->Init(record, new FinishShutdownTask(mHost));
    }
  }

private:
  GMPAsyncShutdownHost* mHost;
};

extern "C" {

HMODULE h264DecoderModule = 0;

GMP_EXPORT GMPErr
GMPInit(GMPPlatformAPI* aPlatformAPI)
{
  assert(aPlatformAPI);
  assert(aPlatformAPI->createthread);
  assert(aPlatformAPI->runonmainthread);
  assert(aPlatformAPI->syncrunonmainthread);
  assert(aPlatformAPI->createmutex);
  sPlatformAPI = aPlatformAPI;

  // TODO: This is unlikely to succeed once the sandbox is enabled....
  SUCCEEDED(CoInitializeEx(0, COINIT_MULTITHREADED));
  const int MF_WIN7_VERSION = (0x0002 << 16 | MF_API_VERSION);
  MFStartup(MF_WIN7_VERSION, MFSTARTUP_FULL);
  h264DecoderModule = LoadLibraryA("msmpeg2vdec.dll");

  return GMPNoErr;
}

GMP_EXPORT GMPErr
GMPGetAPI(const char* aApiName, void* aHostAPI, void** aPluginApi)
{
  if (!strcmp(aApiName, "eme-decrypt")) {
    if (!Decryptor::Get()) {
      Decryptor::Create(reinterpret_cast<GMPDecryptorHost*>(aHostAPI));
    }
    *aPluginApi = Decryptor::Get();
    return GMPNoErr;
  }

  if (!strcmp(aApiName, "decode-video")) {
   *aPluginApi = new VideoDecoder(reinterpret_cast<GMPVideoHost*>(aHostAPI));
   return GMPNoErr;
  }

  if (!strcmp(aApiName, "decode-audio")) {
   *aPluginApi = new AudioDecoder(reinterpret_cast<GMPAudioHost*>(aHostAPI));
   return GMPNoErr;
  }

  if (!strcmp(aApiName, "async-shutdown")) {
    *aPluginApi = new AsyncShutdown(reinterpret_cast<GMPAsyncShutdownHost*>(aHostAPI));
    return GMPNoErr;
  }

  return GMPGenericErr;
}

GMP_EXPORT void
GMPShutdown(void)
{
  FreeLibrary(h264DecoderModule);
  MFShutdown();
  CoUninitialize();

  delete sPlatformAPI;
  sPlatformAPI = nullptr;
}

} // extern "C"
