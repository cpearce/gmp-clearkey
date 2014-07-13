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


class ReadContinuation {
public:
  virtual void OnReadComplete(GMPErr aErr, const std::string& aData) = 0;
};

// Reads a record to storage using GMPRecord.
// Calls ReadContinuation with read data.
GMPErr
ReadRecord(const std::string& aRecordName,
           ReadContinuation* aContinuation);

// Writes a record to storage using GMPRecord.
// Runs continuation when data is written.
GMPErr
WriteRecord(const std::string& aRecordName,
            const std::string& aData,
            GMPTask* aContinuation);

GMPErr
WriteRecord(const std::string& aRecordName,
            const uint8_t* aData,
            uint32_t aNumBytes,
            GMPTask* aContinuation);
