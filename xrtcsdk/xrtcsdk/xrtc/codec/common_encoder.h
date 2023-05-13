#ifndef KRTCSDK_KRTC_CODEC_COMMON_ENCODER_H_
#define KRTCSDK_KRTC_CODEC_COMMON_ENCODER_H_

#include "third_party/openh264/src/codec/api/svc/codec_app_def.h"

namespace xrtc{

// QP scaling thresholds.
static const int kLowH264QpThreshold = 24;
static const int kHighH264QpThreshold = 37;

// Used by histograms. Values of entries should not be changed.
enum NvVideoEncoderEvent
{
	kH264EncoderEventInit = 0,
	kH264EncoderEventError = 1,
	kH264EncoderEventMax = 16,
};

static webrtc::VideoFrameType ConvertToVideoFrameType(EVideoFrameType type) {
	switch (type) {
		case videoFrameTypeIDR:
			return webrtc::VideoFrameType::kVideoFrameKey;
		case videoFrameTypeSkip:
		case videoFrameTypeI:
		case videoFrameTypeP:
		case videoFrameTypeIPMixed:
			return webrtc::VideoFrameType::kVideoFrameDelta;
		case videoFrameTypeInvalid:
			break;
	}
	RTC_NOTREACHED() << "Unexpected/invalid frame type: " << type;
	return webrtc::VideoFrameType::kEmptyFrame;
}

static void RtpFragmentize(webrtc::EncodedImage* encoded_image,
	std::vector<uint8_t>& frame_packet)
{
	size_t required_capacity = 0;
	encoded_image->set_size(0);

	required_capacity = frame_packet.size();
	auto buffer = webrtc::EncodedImageBuffer::Create(required_capacity);
	encoded_image->SetEncodedData(buffer);

	memcpy(buffer->data(), &frame_packet[0], frame_packet.size());
}

} // namespace krtc

#endif // KRTCSDK_KRTC_CODEC_COMMON_ENCODER_H_