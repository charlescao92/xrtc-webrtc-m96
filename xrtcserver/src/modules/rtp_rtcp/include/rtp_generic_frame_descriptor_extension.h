/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_RTP_RTCP_RTP_GENERIC_FRAME_DESCRIPTOR_EXTENSION_H_
#define MODULES_RTP_RTCP_RTP_GENERIC_FRAME_DESCRIPTOR_EXTENSION_H_

#include <stddef.h>
#include <stdint.h>

#include <absl/strings/string_view.h>
#include <api/array_view.h>
#include <api/rtp_parameters.h>
#include <modules/rtp_rtcp/source/rtp_generic_frame_descriptor.h>

#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"

namespace xrtc {

class RtpGenericFrameDescriptorExtension00 {
 public:
  using value_type = webrtc::RtpGenericFrameDescriptor;
  static constexpr RTPExtensionType kId = kRtpExtensionGenericFrameDescriptor00;
  static constexpr absl::string_view Uri() {
    return webrtc::RtpExtension::kGenericFrameDescriptorUri00;
  }
  static constexpr int kMaxSizeBytes = 16;

  static bool Parse(rtc::ArrayView<const uint8_t> data,
                    webrtc::RtpGenericFrameDescriptor* descriptor);
  static size_t ValueSize(const webrtc::RtpGenericFrameDescriptor& descriptor);
  static bool Write(rtc::ArrayView<uint8_t> data,
                    const webrtc::RtpGenericFrameDescriptor& descriptor);
};

}  // namespace xrtc

#endif  // MODULES_RTP_RTCP_SOURCE_RTP_GENERIC_FRAME_DESCRIPTOR_EXTENSION_H_
