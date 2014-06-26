// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "stdafx.h"

#define RCHECK(x) { if (!(x)) return false; }
#define DCHECK(x) RCHECK(x)

static const uint8_t kAnnexBStartCode[] = {0, 0, 0, 1};
static const int kAnnexBStartCodeSize = 4;

static bool
Read1(const uint8_t*& p,
      const uint8_t* end,
      uint8_t& aResult)
{
  ENSURE(p + 1 <= end, false);
  aResult = p[0];
  p++;
  return true;
}

static bool
Read2(const uint8_t*& p,
      const uint8_t* end,
      uint16_t& aResult)
{
  ENSURE(p + 2 <= end, false);
  aResult = (uint16_t(p[0]) << 8) + (uint16_t)(p[1]);
  p += 2;
  return true;
}

static bool
ReadVec(const uint8_t*& p,
        const uint8_t* end,
        std::vector<uint8_t>& vec,
        int count)
{
  ENSURE(p + count <= end, false);
  vec.resize(count);
  uint32_t bytesRead = 0;
  vec.insert(vec.end(), p, p+count);
  p += count;
  return true;
}

bool
AVCDecoderConfigurationRecord::Parse(const std::vector<uint8_t>& avcc)
{
  // AVCC 6 byte header looks like:
  //     +------+------+------+------+------+------+------+------+
  // [0] |   0  |   0  |   0  |   0  |   0  |   0  |   0  |   1  |
  //     +------+------+------+------+------+------+------+------+
  // [1] | profile                                               |
  //     +------+------+------+------+------+------+------+------+
  // [2] | compatiblity                                          |
  //     +------+------+------+------+------+------+------+------+
  // [3] | level                                                 |
  //     +------+------+------+------+------+------+------+------+
  // [4] | unused                                  | nalLenSiz-1 |
  //     +------+------+------+------+------+------+------+------+
  // [5] | unused             | numSps                           |
  //     +------+------+------+------+------+------+------+------+

  ENSURE(avcc.size() >= 6, false);
  version = avcc[0];
  ENSURE(version == 1, false);
  profile_indication = avcc[1];
  profile_compatibility = avcc[2];
  avc_level = avcc[3];
  nal_length_size = 1 + (avcc[4] & 3);
  assert(nal_length_size == 4); // Gecko's demuxer should guarantee this.

  uint8_t num_sps = avcc[5] & 31;
  sps_list.resize(num_sps);
  const uint8_t* d = avcc.data() + 6;
  const uint8_t* end = avcc.data() + avcc.size();
  for (int i = 0; i < num_sps; i++) {
    uint16_t sps_length;
    RCHECK(Read2(d, end, sps_length));
    RCHECK(ReadVec(d, end, sps_list[i], sps_length));
  }

  uint8_t num_pps;
  RCHECK(Read1(d, end, num_pps));
  pps_list.resize(num_pps);
  for (int i = 0; i < num_pps; i++) {
    uint16_t pps_length;
    RCHECK(Read2(d, end, pps_length) &&
           ReadVec(d, end, pps_list[i], pps_length));
  }

  return true;
}

static bool ConvertAVCToAnnexBInPlaceForLengthSize4(std::vector<uint8_t>* buf) {
  const int kLengthSize = 4;
  size_t pos = 0;
  while (pos + kLengthSize < buf->size()) {
    int nal_size = (*buf)[pos];
    nal_size = (nal_size << 8) + (*buf)[pos+1];
    nal_size = (nal_size << 8) + (*buf)[pos+2];
    nal_size = (nal_size << 8) + (*buf)[pos+3];
    std::copy(kAnnexBStartCode, kAnnexBStartCode + kAnnexBStartCodeSize,
              buf->begin() + pos);
    pos += kLengthSize + nal_size;
  }
  return pos == buf->size();
}

// static
bool AVC::ConvertFrameToAnnexB(int nal_length_size, std::vector<uint8_t>* buffer) {
  RCHECK(nal_length_size == 1 || nal_length_size == 2 || nal_length_size == 4);

  if (nal_length_size == 4)
    return ConvertAVCToAnnexBInPlaceForLengthSize4(buffer);

  std::vector<uint8_t> temp;
  temp.swap(*buffer);
  buffer->reserve(temp.size() + 32);

  size_t pos = 0;
  while (pos + nal_length_size < temp.size()) {
    int nal_size = temp[pos];
    if (nal_length_size == 2) nal_size = (nal_size << 8) + temp[pos+1];
    pos += nal_length_size;

    RCHECK(pos + nal_size <= temp.size());
    buffer->insert(buffer->end(), kAnnexBStartCode,
                   kAnnexBStartCode + kAnnexBStartCodeSize);
    buffer->insert(buffer->end(), temp.begin() + pos,
                   temp.begin() + pos + nal_size);
    pos += nal_size;
  }
  return pos == temp.size();
}

// static
bool AVC::ConvertConfigToAnnexB(const AVCDecoderConfigurationRecord& avc_config,
                                std::vector<uint8_t>* buffer) {
  DCHECK(buffer->empty());
  buffer->clear();
  int total_size = 0;
  for (size_t i = 0; i < avc_config.sps_list.size(); i++)
    total_size += avc_config.sps_list[i].size() + kAnnexBStartCodeSize;
  for (size_t i = 0; i < avc_config.pps_list.size(); i++)
    total_size += avc_config.pps_list[i].size() + kAnnexBStartCodeSize;
  buffer->reserve(total_size);

  for (size_t i = 0; i < avc_config.sps_list.size(); i++) {
    buffer->insert(buffer->end(), kAnnexBStartCode,
                kAnnexBStartCode + kAnnexBStartCodeSize);
    buffer->insert(buffer->end(), avc_config.sps_list[i].begin(),
                avc_config.sps_list[i].end());
  }

  for (size_t i = 0; i < avc_config.pps_list.size(); i++) {
    buffer->insert(buffer->end(), kAnnexBStartCode,
                   kAnnexBStartCode + kAnnexBStartCodeSize);
    buffer->insert(buffer->end(), avc_config.pps_list[i].begin(),
                   avc_config.pps_list[i].end());
  }
  return true;
}
