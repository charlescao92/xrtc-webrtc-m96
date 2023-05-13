#ifndef KRTCSDK_KRTC_CODEC_EXTERNAL_BUILTIN_VIDEO_ENCODER_FACTORY_H_
#define KRTCSDK_KRTC_CODEC_EXTERNAL_BUILTIN_VIDEO_ENCODER_FACTORY_H_

#include <memory>

#include "rtc_base/system/rtc_export.h"
#include "api/video_codecs/video_encoder_factory.h"

namespace xrtc {

	// Creates a new factory that can create the built-in types of video decoders.
	std::unique_ptr<webrtc::VideoEncoderFactory> CreateBuiltinExternalVideoEncoderFactory();

} // namespace xrtc

#endif  // KRTCSDK_KRTC_CODEC_EXTERNAL_BUILTIN_VIDEO_ENCODER_FACTORY_H_