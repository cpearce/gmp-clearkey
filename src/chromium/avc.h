// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MP4_AVC_H_
#define MEDIA_MP4_AVC_H_

struct AVCDecoderConfigurationRecord {
  uint8_t version;
  uint8_t profile_indication;
  uint8_t profile_compatibility;
  uint8_t avc_level;
  uint8_t nal_length_size;

  bool Parse(const std::vector<uint8_t>& aAVCC);

  typedef std::vector<uint8_t> SPS;
  typedef std::vector<uint8_t> PPS;

  std::vector<SPS> sps_list;
  std::vector<PPS> pps_list;
};

class AVC {
 public:
  static bool ConvertFrameToAnnexB(int nal_length_size,
                                   std::vector<uint8_t>* buffer);

  static bool ConvertConfigToAnnexB(
      const AVCDecoderConfigurationRecord& avc_config,
      std::vector<uint8_t>* annex_b);
};

#endif  // MEDIA_MP4_AVC_H_
