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

#ifdef TEST_DECODING

#if !defined(WMFAACDecoder_h_)
#define WMFAACDecoder_h_

class WMFAACDecoder {
public:
  WMFAACDecoder();
  ~WMFAACDecoder();

  HRESULT Init(int32_t aChannelCount,
               int32_t aSampleRate,
               BYTE* aUserData,
               UINT32 aUserDataLength);

  HRESULT Input(const uint8_t* aData,
                uint32_t aDataSize,
                Microseconds aTimestamp);

  HRESULT Output(IMFSample** aOutput);

  HRESULT Reset();

private:

  HRESULT GetOutputSample(IMFSample** aOutSample);
  HRESULT CreateOutputSample(IMFSample** aOutSample);
  HRESULT CreateInputSample(const uint8_t* aData,
                            uint32_t aDataSize,
                            Microseconds aTimestamp,
                            IMFSample** aOutSample);

  HRESULT SetDecoderInputType(int32_t aChannelCount,
                              int32_t aSampleRate,
                              BYTE* aUserData,
                              UINT32 aUserDataLength);
  HRESULT SetDecoderOutputType();
  HRESULT SendMFTMessage(MFT_MESSAGE_TYPE aMsg, UINT32 aData);

  MFT_INPUT_STREAM_INFO mInputStreamInfo;
  MFT_OUTPUT_STREAM_INFO mOutputStreamInfo;

  CComPtr<IMFTransform> mDecoder;
};

#endif

#endif
