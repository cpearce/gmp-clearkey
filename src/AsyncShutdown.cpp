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

#ifdef TEST_GMP_ASYNC_SHUTDOWN

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


AsyncShutdown::AsyncShutdown(GMPAsyncShutdownHost* aHost)
  : mHost(aHost)
{}

void AsyncShutdown::BeginShutdown() {
  GMPTimestamp t;
  auto err = GMPGetCurrentTime(&t);
  std::string msg;
  if (GMP_SUCCEEDED(err)) {
    msg = std::to_string(t);
  }
  err = WriteRecord(SHUTDOWN_TIME_RECORD,
                    msg,
                    new FinishShutdownTask(mHost));
}

#endif
