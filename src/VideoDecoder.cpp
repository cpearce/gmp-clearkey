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

VideoDecoder::VideoDecoder(GMPVideoHost *aHostAPI)
  : mHostAPI(aHostAPI)
  , mCallback(nullptr)
  , mWorkerThread(nullptr)
  , mMutex(nullptr)
  , mNumInputTasks(0)
  , mSentExtraData(false)
{
}

VideoDecoder::~VideoDecoder()
{
}

GMPVideoErr
VideoDecoder::InitDecode(const GMPVideoCodec& aCodecSettings,
                         GMPVideoDecoderCallback* aCallback,
                         int32_t aCoreCount)
{
  mCallback = aCallback;
  assert(mCallback);
  mDecoder = new WMFH264Decoder();
  HRESULT hr = mDecoder->Init();
  ENSURE(SUCCEEDED(hr), GMPVideoGenericErr);

  auto err = GMPCreateMutex(mMutex.Receive());
  ENSURE(GMP_SUCCEEDED(err), GMPVideoGenericErr);

  mExtraData.insert(mExtraData.end(),
                    aCodecSettings.mExtraData,
                    aCodecSettings.mExtraData + aCodecSettings.mExtraDataLen);

  if (!mAVCC.Parse(mExtraData) ||
      !AVC::ConvertConfigToAnnexB(mAVCC, &mAnnexB)) {
    return GMPVideoGenericErr;
  }

  return GMPVideoNoErr;
}

GMPVideoErr
VideoDecoder::Decode(GMPVideoEncodedFrame* aInputFrame,
                     bool aMissingFrames,
                     const GMPCodecSpecificInfo& aCodecSpecificInfo,
                     int64_t aRenderTimeMs)
{
  if (!mWorkerThread) {
    GMPCreateThread(mWorkerThread.Receive());
    if (!mWorkerThread) {
      return GMPVideoAllocErr;
    }
  }
  {
    AutoLock lock(mMutex);
    mNumInputTasks++;
  }
  mWorkerThread->Post(WrapTask(this,
                               &VideoDecoder::DecodeTask,
                               aInputFrame,
                               aCodecSpecificInfo));
  return GMPVideoNoErr;
}

static const uint8_t kAnnexBDelimiter[] = { 0, 0, 0, 1 };

void
VideoDecoder::DecodeTask(GMPVideoEncodedFrame* aInput,
                         const GMPCodecSpecificInfo& aCodecSpecificInfo)
{
  HRESULT hr;

  {
    AutoLock lock(mMutex);
    mNumInputTasks--;
    assert(mNumInputTasks >= 0);
  }

  if (!aInput || !mHostAPI || !mDecoder) {
    LOG(L"Decode job not set up correctly!");
    return;
  }

  const uint8_t* inBuffer = aInput->Buffer();
  if (!inBuffer) {
    LOG(L"No buffer for encoded frame!\n");
    return;
  }

  const GMPEncryptedBufferData* crypto = aInput->GetDecryptionData();
  std::vector<uint8_t> buffer;
  if (crypto) {
    Decryptor d;
    if (!d.Decrypt(inBuffer, aInput->Size(), crypto, buffer)) {
      LOG(L"Video decryption error!");
      // TODO: Report error...
      return;
    }
    buffer.data();
  } else {
    buffer.insert(buffer.end(), inBuffer, inBuffer+aInput->Size());
  }

  AVC::ConvertFrameToAnnexB(4, &buffer);
  if (aInput->FrameType() == kGMPKeyFrame) {
    // We must send the SPS and PPS to Windows Media Foundation's decoder.
    // Note: We do this *after* decryption, otherwise the subsample info
    // would be incorrect.
    buffer.insert(buffer.begin(), mAnnexB.begin(), mAnnexB.end());
  }

  hr = mDecoder->Input(buffer.data(),
                       buffer.size(),
                       aInput->TimeStamp(),
                       aInput->CaptureTime());

  // We must delete the input sample!
  GMPSyncRunOnMainThread(WrapTask(aInput, &GMPVideoEncodedFrame::Destroy));

  SAMPLE_LOG(L"VideoDecoder::DecodeTask() Input ret hr=0x%x\n", hr);
  if (FAILED(hr)) {
    LOG(L"VideoDecoder::DecodeTask() decode failed ret=0x%x%s\n",
        hr,
        ((hr == MF_E_NOTACCEPTING) ? " (MF_E_NOTACCEPTING)" : ""));
    return;
  }

  while (hr == S_OK) {
    CComPtr<IMFSample> output;
    hr = mDecoder->Output(&output);
    SAMPLE_LOG(L"VideoDecoder::DecodeTask() output ret=0x%x\n", hr);
    if (hr == S_OK) {
      ReturnOutput(output);
    }
    if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
      AutoLock lock(mMutex);
      if (mNumInputTasks == 0) {
        // We have run all input tasks. We *must* notify Gecko so that it will
        // send us more data.
        GMPSyncRunOnMainThread(WrapTask(mCallback, &GMPVideoDecoderCallback::InputDataExhausted));
      }
    }
    if (FAILED(hr)) {
      LOG(L"VideoDecoder::DecodeTask() output failed hr=0x%x\n", hr);
    }
  }
}

void
VideoDecoder::ReturnOutput(IMFSample* aSample)
{
  SAMPLE_LOG(L"[%p] WMFDecodingModule::OutputVideoFrame()\n", this);
  assert(aSample);

  HRESULT hr;

  GMPVideoFrame* f = nullptr;
  mHostAPI->CreateFrame(kGMPI420VideoFrame, &f);
  if (!f) {
    LOG(L"Failed to create i420 frame!\n");
    return;
  }
  auto vf = static_cast<GMPVideoi420Frame*>(f);

  hr = SampleToVideoFrame(aSample, vf);
  ENSURE(SUCCEEDED(hr), /*void*/);

  GMPSyncRunOnMainThread(WrapTask(mCallback, &GMPVideoDecoderCallback::Decoded, vf));

}

HRESULT
VideoDecoder::SampleToVideoFrame(IMFSample* aSample,
                                 GMPVideoi420Frame* aVideoFrame)
{
  ENSURE(aSample != nullptr, E_POINTER);
  ENSURE(aVideoFrame != nullptr, E_POINTER);

  HRESULT hr;
  CComPtr<IMFMediaBuffer> mediaBuffer;

  // Must convert to contiguous mediaBuffer to use IMD2DBuffer interface.
  hr = aSample->ConvertToContiguousBuffer(&mediaBuffer);
  ENSURE(SUCCEEDED(hr), hr);

  // Try and use the IMF2DBuffer interface if available, otherwise fallback
  // to the IMFMediaBuffer interface. Apparently IMF2DBuffer is more efficient,
  // but only some systems (Windows 8?) support it.
  BYTE* data = nullptr;
  LONG stride = 0;
  CComPtr<IMF2DBuffer> twoDBuffer;
  hr = mediaBuffer->QueryInterface(static_cast<IMF2DBuffer**>(&twoDBuffer));
  if (SUCCEEDED(hr)) {
    hr = twoDBuffer->Lock2D(&data, &stride);
    ENSURE(SUCCEEDED(hr), hr);
  } else {
    hr = mediaBuffer->Lock(&data, NULL, NULL);
    ENSURE(SUCCEEDED(hr), hr);
    stride = mDecoder->GetStride();
  }
  int32_t width = mDecoder->GetFrameWidth();
  int32_t height = mDecoder->GetFrameHeight();

  // The V and U planes are stored 16-row-aligned, so we need to add padding
  // to the row heights to ensure the Y'CbCr planes are referenced properly.
  // YV12, planar format: [YYYY....][VVVV....][UUUU....]
  // i.e., Y, then V, then U.
  uint32_t padding = 0;
  if (height % 16 != 0) {
    padding = 16 - (height % 16);
  }
  int32_t y_size = stride * (height + padding);
  int32_t v_size = stride * (height + padding) / 4;
  int32_t halfStride = (stride + 1) / 2;
  int32_t halfHeight = (height + 1) / 2;
  int32_t halfWidth = (width + 1) / 2;
  int32_t totalSize = y_size + 2 * v_size;

  GMPSyncRunOnMainThread(WrapTask(aVideoFrame,
                                  &GMPVideoi420Frame::CreateEmptyFrame,
                                  stride, height, stride, halfStride, halfStride));

  auto err = aVideoFrame->SetWidth(width);
  ENSURE(GMP_SUCCEEDED(err), E_FAIL);
  err = aVideoFrame->SetHeight(height);
  ENSURE(GMP_SUCCEEDED(err), E_FAIL);

  uint8_t* outBuffer = aVideoFrame->Buffer(kGMPYPlane);
  ENSURE(outBuffer != nullptr, E_FAIL);
  assert(aVideoFrame->AllocatedSize(kGMPYPlane) >= stride*height);
  memcpy(outBuffer, data, stride*height);

  outBuffer = aVideoFrame->Buffer(kGMPUPlane);
  ENSURE(outBuffer != nullptr, E_FAIL);
  assert(aVideoFrame->AllocatedSize(kGMPUPlane) >= halfStride*halfHeight);
  memcpy(outBuffer, data+y_size, halfStride*halfHeight);

  outBuffer = aVideoFrame->Buffer(kGMPVPlane);
  ENSURE(outBuffer != nullptr, E_FAIL);
  assert(aVideoFrame->AllocatedSize(kGMPVPlane) >= halfStride*halfHeight);
  memcpy(outBuffer, data + y_size + v_size, halfStride*halfHeight);

  if (twoDBuffer) {
    twoDBuffer->Unlock2D();
  } else {
    mediaBuffer->Unlock();
  }

  LONGLONG hns = 0;
  hr = aSample->GetSampleTime(&hns);
  ENSURE(SUCCEEDED(hr), hr);
  aVideoFrame->SetTimestamp(HNsToUsecs(hns));

  hr = aSample->GetSampleDuration(&hns);
  ENSURE(SUCCEEDED(hr), hr);
  // HACK: Set the render time as the duration. Need to fix the API.
  aVideoFrame->SetRenderTime_ms(HNsToUsecs(hns));

  return S_OK;
}

GMPVideoErr
VideoDecoder::Reset()
{
  return GMPVideoNoErr;
}

GMPVideoErr
VideoDecoder::Drain()
{
  return GMPVideoNoErr;
}

void
VideoDecoder::DecodingComplete()
{
  if (mWorkerThread) {
    mWorkerThread->Join();
  }
  delete this;
}
