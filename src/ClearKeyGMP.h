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

GMPErr GMPCreateThread(GMPThread** aThread);
GMPErr GMPRunOnMainThread(GMPTask* aTask);
GMPErr GMPSyncRunOnMainThread(GMPTask* aTask);
GMPErr GMPCreateMutex(GMPMutex** aMutex);
GMPErr GMPOpenRecord(const char* aName,
                     uint32_t aNameLength,
                     GMPRecord** aOutRecord,
                     GMPRecordClient* aClient);
GMPErr GMPSetTimer(GMPTask* aTask, int64_t aTimeoutMS);
GMPErr GMPGetCurrentTime(GMPTimestamp* aOutTime);

// Turn on to disable usage of GMPStorage, GMPTimer, GMPAsyncShutdown,
// and A/V decoding.
#define DECRYPT_DATA_ONLY 1
