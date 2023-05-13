/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_RTP_VIDEO_LAYERS_ALLOCATION_EXTENSION_H_
#define MODULES_RTP_RTCP_SOURCE_RTP_VIDEO_LAYERS_ALLOCATION_EXTENSION_H_

#include <stdlib.h>

#include <api/array_view.h>
#include <absl/strings/string_view.h>
#include <api/rtp_parameters.h>
#include <api/video/video_layers_allocation.h>

#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"

namespace xrtc {

// TODO(bugs.webrtc.org/12000): Note that this extensions is being developed and
// the wire format will likely change.
class RtpVideoLayersAllocationExtension {
 public:
  using value_type = webrtc::VideoLayersAllocation;
  static constexpr RTPExtensionType kId = kRtpExtensionVideoLayersAllocation;
  static constexpr absl::string_view Uri() {
    return webrtc::RtpExtension::kVideoLayersAllocationUri;
  }

  static bool Parse(rtc::ArrayView<const uint8_t> data,
                    webrtc::VideoLayersAllocation* allocation);
  static size_t ValueSize(const webrtc::VideoLayersAllocation& allocation);
  static bool Write(rtc::ArrayView<uint8_t> data,
                    const webrtc::VideoLayersAllocation& allocation);
};

}  // namespace xrtc
#endif  // MODULES_RTP_RTCP_SOURCE_RTP_VIDEO_LAYERS_ALLOCATION_EXTENSION_H_
