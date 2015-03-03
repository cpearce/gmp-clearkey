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

#ifdef TEST_DECODING
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "wmcodecdspuuid")
#pragma comment(lib, "mfplat.lib")
#endif

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
GMPCreateMutex(GMPMutex** aMutex)
{
  assert(sPlatformAPI);
  return sPlatformAPI->createmutex(aMutex);
}

#ifdef TEST_GMP_STORAGE
GMPErr
GMPOpenRecord(const char* aName,
              uint32_t aNameLength,
              GMPRecord** aOutRecord,
              GMPRecordClient* aClient)
{
  return sPlatformAPI->createrecord(aName, aNameLength, aOutRecord, aClient);
}
#endif

#ifdef TEST_GMP_TIMER
GMPErr
GMPSetTimer(GMPTask* aTask, int64_t aTimeoutMS)
{
  return sPlatformAPI->settimer(aTask, aTimeoutMS);
}
#endif

GMPErr
GMPGetCurrentTime(GMPTimestamp* aOutTime)
{
  return sPlatformAPI->getcurrenttime(aOutTime);
}

#ifdef TEST_NODE_ID
static std::string sNodeId;

const std::string& GetNodeId() {
  return sNodeId;
}
#endif // TEST_NODE_ID

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

#ifdef TEST_DECODING
  // TODO: This is unlikely to succeed once the sandbox is enabled....
  SUCCEEDED(CoInitializeEx(0, COINIT_MULTITHREADED));
  const int MF_WIN7_VERSION = (0x0002 << 16 | MF_API_VERSION);
  MFStartup(MF_WIN7_VERSION, MFSTARTUP_FULL);
  h264DecoderModule = LoadLibraryA("msmpeg2vdec.dll");
#endif

  return GMPNoErr;
}

GMP_EXPORT GMPErr
GMPGetAPI(const char* aApiName, void* aHostAPI, void** aPluginApi)
{
  if (!strcmp(aApiName, GMP_API_DECRYPTOR)) {
    *aPluginApi = new Decryptor(reinterpret_cast<GMPDecryptorHost*>(aHostAPI));
    return GMPNoErr;
  }

#ifdef TEST_DECODING
  if (!strcmp(aApiName, GMP_API_VIDEO_DECODER)) {
   *aPluginApi = new VideoDecoder(reinterpret_cast<GMPVideoHost*>(aHostAPI));
   return GMPNoErr;
  }

  if (!strcmp(aApiName, GMP_API_AUDIO_DECODER)) {
   *aPluginApi = new AudioDecoder(reinterpret_cast<GMPAudioHost*>(aHostAPI));
   return GMPNoErr;
  }
#endif

#ifdef TEST_GMP_ASYNC_SHUTDOWN
  if (!strcmp(aApiName, GMP_API_ASYNC_SHUTDOWN)) {
    *aPluginApi = new AsyncShutdown(reinterpret_cast<GMPAsyncShutdownHost*>(aHostAPI));
    return GMPNoErr;
  }
#endif

  return GMPGenericErr;
}

GMP_EXPORT void
GMPShutdown(void)
{
#ifdef TEST_DECODING
  FreeLibrary(h264DecoderModule);
  MFShutdown();
  CoUninitialize();
#endif

  sPlatformAPI = nullptr;
}

#ifdef TEST_NODE_ID
GMP_EXPORT void
GMPSetNodeId(const char* aNodeId, uint32_t aLength)
{
  sNodeId = std::string(aNodeId, aNodeId + aLength);
}
#endif // TEST_NODE_ID

} // extern "C"
