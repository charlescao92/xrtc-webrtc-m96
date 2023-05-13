#include "qsv_encoder.h"
#include "common_encoder.h"

#include <limits>
#include <string>

#include "absl/strings/match.h"
#include "common_video/h264/h264_common.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "modules/video_coding/utility/simulcast_rate_allocator.h"
#include "modules/video_coding/utility/simulcast_utility.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/time_utils.h"
#include "system_wrappers/include/metrics.h"
#include "third_party/libyuv/include/libyuv/convert.h"
#include "third_party/libyuv/include/libyuv/scale.h"
#include "third_party/libyuv/include/libyuv/video_common.h"

namespace xrtc {

QsvEncoder::QsvEncoder(const cricket::VideoCodec& codec)
	: packetization_mode_(webrtc::H264PacketizationMode::SingleNalUnit),
	max_payload_size_(0),
	number_of_cores_(0),
	encoded_image_callback_(nullptr),
	has_reported_init_(false),
	has_reported_error_(false),
	num_temporal_layers_(1)
{
	RTC_CHECK(absl::EqualsIgnoreCase(codec.name, cricket::kH264CodecName));
	std::string packetization_mode_string;
	if (codec.GetParam(cricket::kH264FmtpPacketizationMode, &packetization_mode_string)
		&& packetization_mode_string == "1") {
		packetization_mode_ = webrtc::H264PacketizationMode::NonInterleaved;
	}

	encoded_images_.reserve(webrtc::kMaxSimulcastStreams);
	qsv_encoders_.reserve(webrtc::kMaxSimulcastStreams);
	configurations_.reserve(webrtc::kMaxSimulcastStreams);
	tl0sync_limit_.reserve(webrtc::kMaxSimulcastStreams);
	image_buffer_ = nullptr;
}

QsvEncoder::~QsvEncoder()
{
	Release();
}

int32_t QsvEncoder::InitEncode(const webrtc::VideoCodec* inst,
	int32_t number_of_cores,
	size_t max_payload_size)
{
	ReportInit();
	if (!inst || inst->codecType != webrtc::kVideoCodecH264) {
		ReportError();
		return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
	}
	if (inst->maxFramerate == 0) {
		ReportError();
		return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
	}
	if (inst->width < 1 || inst->height < 1) {
		ReportError();
		return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
	}

	int32_t release_ret = Release();
	if (release_ret != WEBRTC_VIDEO_CODEC_OK) {
		ReportError();
		return release_ret;
	}

	int number_of_streams = webrtc::SimulcastUtility::NumberOfSimulcastStreams(*inst);
	bool doing_simulcast = (number_of_streams > 1);

	if (doing_simulcast && !webrtc::SimulcastUtility::ValidSimulcastParameters(*inst, number_of_streams)) {
		return WEBRTC_VIDEO_CODEC_ERR_SIMULCAST_PARAMETERS_NOT_SUPPORTED;
	}

	assert(number_of_streams == 1);

	encoded_images_.resize(number_of_streams);
	qsv_encoders_.resize(number_of_streams);
	configurations_.resize(number_of_streams);
	tl0sync_limit_.resize(number_of_streams);

	number_of_cores_ = number_of_cores;
	max_payload_size_ = max_payload_size;
	codec_ = *inst;

	// Code expects simulcastStream resolutions to be correct, make sure they are
	// filled even when there are no simulcast layers.
	if (codec_.numberOfSimulcastStreams == 0) {
		codec_.simulcastStream[0].width = codec_.width;
		codec_.simulcastStream[0].height = codec_.height;
	}

	num_temporal_layers_ = codec_.H264()->numberOfTemporalLayers;

	for (int i = 0, idx = number_of_streams - 1; i < number_of_streams; ++i, --idx) {
		// Store nvidia encoder.
		xop::IntelD3DEncoder* qsv_encoder = new xop::IntelD3DEncoder();
		qsv_encoders_[i] = qsv_encoder;

		// Set internal settings from codec_settings
		configurations_[i].simulcast_idx = idx;
		configurations_[i].sending = false;
		configurations_[i].width = codec_.simulcastStream[idx].width;
		configurations_[i].height = codec_.simulcastStream[idx].height;
		configurations_[i].max_frame_rate = static_cast<float>(codec_.maxFramerate);
		configurations_[i].frame_dropping_on = codec_.H264()->frameDroppingOn;
		configurations_[i].key_frame_interval = codec_.H264()->keyFrameInterval;
		configurations_[i].num_temporal_layers =
			std::max(codec_.H264()->numberOfTemporalLayers,
				codec_.simulcastStream[idx].numberOfTemporalLayers);

		// Codec_settings uses kbits/second; encoder uses bits/second.
		configurations_[i].max_bps = codec_.maxBitrate * 1000;
		configurations_[i].target_bps = codec_.maxBitrate * 1000 / 2;

		qsv_encoder->SetOption(xop::VE_OPT_WIDTH, configurations_[i].width);
		qsv_encoder->SetOption(xop::VE_OPT_HEIGHT, configurations_[i].height);
		qsv_encoder->SetOption(xop::VE_OPT_FRAME_RATE, static_cast<int>(configurations_[i].max_frame_rate));
		qsv_encoder->SetOption(xop::VE_OPT_GOP, configurations_[i].key_frame_interval);
		qsv_encoder->SetOption(xop::VE_OPT_CODEC, xop::VE_OPT_CODEC_H264);
		qsv_encoder->SetOption(xop::VE_OPT_BITRATE_KBPS, configurations_[i].target_bps / 1000);
		qsv_encoder->SetOption(xop::VE_OPT_TEXTURE_FORMAT, xop::VE_OPT_FORMAT_NV12);
		if (!qsv_encoder->Init()) {
			Release();
			ReportError();
			return WEBRTC_VIDEO_CODEC_ERROR;
		}

		image_buffer_.reset(new uint8_t[configurations_[i].width * configurations_[i].height * 10]);

		// TODO(pbos): Base init params on these values before submitting.
		video_format_ = EVideoFormatType::videoFormatI420;

		// Initialize encoded image. Default buffer size: size of unencoded data.
		const size_t new_capacity = webrtc::CalcBufferSize(webrtc::VideoType::kI420,
			codec_.simulcastStream[idx].width, codec_.simulcastStream[idx].height);
		encoded_images_[i].SetEncodedData(webrtc::EncodedImageBuffer::Create(new_capacity));
		encoded_images_[i]._encodedWidth = codec_.simulcastStream[idx].width;
		encoded_images_[i]._encodedHeight = codec_.simulcastStream[idx].height;
		encoded_images_[i].set_size(0);

		tl0sync_limit_[i] = configurations_[i].num_temporal_layers;
	}

	webrtc::SimulcastRateAllocator init_allocator(codec_);
	webrtc::VideoBitrateAllocation allocation = init_allocator.GetAllocation(
		codec_.maxBitrate * 1000 / 2, codec_.maxFramerate);
	SetRates(RateControlParameters(allocation, codec_.maxFramerate));
	return WEBRTC_VIDEO_CODEC_OK;
}

int32_t QsvEncoder::Release()
{
	while (!qsv_encoders_.empty())
	{
		xop::IntelD3DEncoder* qsv_encoder = reinterpret_cast<xop::IntelD3DEncoder*>(qsv_encoders_.back());
		if (qsv_encoder) {
			qsv_encoder->Destroy();
			delete qsv_encoder;
		}
		qsv_encoders_.pop_back();
	}

	configurations_.clear();
	encoded_images_.clear();
	tl0sync_limit_.clear();

	return WEBRTC_VIDEO_CODEC_OK;
}

int32_t QsvEncoder::RegisterEncodeCompleteCallback(webrtc::EncodedImageCallback * callback)
{
	encoded_image_callback_ = callback;
	return WEBRTC_VIDEO_CODEC_OK;
}

void QsvEncoder::SetRates(const RateControlParameters& parameters)
{
	if (qsv_encoders_.empty()) {
		RTC_LOG(LS_WARNING) << "SetRates() while uninitialized.";
		return;
	}

	if (parameters.framerate_fps < 1.0) {
		RTC_LOG(LS_WARNING) << "Invalid frame rate: " << parameters.framerate_fps;
		return;
	}

	if (parameters.bitrate.get_sum_bps() == 0) {
		// Encoder paused, turn off all encoding.
		for (size_t i = 0; i < configurations_.size(); ++i)
			configurations_[i].SetStreamState(false);
		return;
	}

	codec_.maxFramerate = static_cast<uint32_t>(parameters.framerate_fps);

	size_t stream_idx = qsv_encoders_.size() - 1;
	for (size_t i = 0; i < qsv_encoders_.size(); ++i, --stream_idx) {
		configurations_[i].target_bps = parameters.bitrate.GetSpatialLayerSum(stream_idx);
		configurations_[i].max_frame_rate = static_cast<float>(parameters.framerate_fps);

		if (configurations_[i].target_bps) {
			configurations_[i].SetStreamState(true);

			if (qsv_encoders_[i]) {
				xop::IntelD3DEncoder* qsv_encoder = reinterpret_cast<xop::IntelD3DEncoder*>(qsv_encoders_[i]);
				qsv_encoder->SetEvent(xop::VE_EVENT_RESET_BITRATE_KBPS, configurations_[i].target_bps / 1000);
				qsv_encoder->SetEvent(xop::VE_EVENT_RESET_FRAME_RATE, static_cast<int>(configurations_[i].max_frame_rate));
			}
			else {
				configurations_[i].SetStreamState(false);
			}
		}
	}
}

int32_t QsvEncoder::Encode(const webrtc::VideoFrame& input_frame,
	const std::vector<webrtc::VideoFrameType>* frame_types)
{
	if (qsv_encoders_.empty()) {
		ReportError();
		return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
	}

	if (!encoded_image_callback_) {
		RTC_LOG(LS_WARNING)
			<< "InitEncode() has been called, but a callback function "
			<< "has not been set with RegisterEncodeCompleteCallback()";
		ReportError();
		return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
	}

	rtc::scoped_refptr<const webrtc::I420BufferInterface> frame_buffer = 
		input_frame.video_frame_buffer()->ToI420();
	if (!frame_buffer) {
		RTC_LOG(LS_ERROR) << "Failed to convert "
			<< VideoFrameBufferTypeToString(
				input_frame.video_frame_buffer()->type())
			<< " image to I420. Can't encode frame.";
		return WEBRTC_VIDEO_CODEC_ENCODER_FAILURE;
	}
	RTC_CHECK(frame_buffer->type() == webrtc::VideoFrameBuffer::Type::kI420 ||
		frame_buffer->type() == webrtc::VideoFrameBuffer::Type::kI420A);

	bool send_key_frame = false;
	for (size_t i = 0; i < configurations_.size(); ++i) {
		if (configurations_[i].key_frame_request && configurations_[i].sending) {
			send_key_frame = true;
			break;
		}
	}

	if (!send_key_frame && frame_types) {
		for (size_t i = 0; i < configurations_.size(); ++i) {
			const size_t simulcast_idx =
				static_cast<size_t>(configurations_[i].simulcast_idx);
			if (configurations_[i].sending && simulcast_idx < frame_types->size() &&
				(*frame_types)[simulcast_idx] == webrtc::VideoFrameType::kVideoFrameKey) {
				send_key_frame = true;
				break;
			}
		}
	}

	RTC_DCHECK_EQ(configurations_[0].width, frame_buffer->width());
	RTC_DCHECK_EQ(configurations_[0].height, frame_buffer->height());

	// Encode image for each layer.
	for (size_t i = 0; i < qsv_encoders_.size(); ++i) {
		if (!configurations_[i].sending) {
			continue;
		}

		if (frame_types != nullptr) {
			// Skip frame?
			if ((*frame_types)[i] == webrtc::VideoFrameType::kEmptyFrame) {
				continue;
			}
		}

		if (send_key_frame) {
			if (!qsv_encoders_.empty() && qsv_encoders_[i]) {
				xop::IntelD3DEncoder* qsv_encoder = reinterpret_cast<xop::IntelD3DEncoder*>(qsv_encoders_[i]);
				qsv_encoder->SetEvent(xop::VE_EVENT_FORCE_IDR, 1);
			}

			configurations_[i].key_frame_request = false;
		}

		// EncodeFrame output.
		SFrameBSInfo info;
		memset(&info, 0, sizeof(SFrameBSInfo));
		std::vector<uint8_t> frame_packet;

		bool enc_ret = EncodeFrame((int)i, input_frame, frame_packet);
		if (!enc_ret) {
			RTC_LOG(LS_ERROR)
				<< "OpenH264 frame encoding failed";
			ReportError();
			return WEBRTC_VIDEO_CODEC_ERROR;
		}

		if (frame_packet.size() == 0) {
			return WEBRTC_VIDEO_CODEC_OK;
		}
		else {
			if ((frame_packet[4] & 0x1f) == 0x07) {
				// sps + pps + idr
				info.eFrameType = videoFrameTypeIDR;
			}
			else {
				info.eFrameType = videoFrameTypeP;
			}
		}

		encoded_images_[i]._encodedWidth = configurations_[i].width;
		encoded_images_[i]._encodedHeight = configurations_[i].height;
		encoded_images_[i].SetTimestamp(input_frame.timestamp());
		encoded_images_[i].ntp_time_ms_ = input_frame.ntp_time_ms();
		encoded_images_[i].capture_time_ms_ = input_frame.render_time_ms();
		encoded_images_[i].rotation_ = input_frame.rotation();
		encoded_images_[i].SetColorSpace(input_frame.color_space());
		encoded_images_[i].content_type_ = (codec_.mode == webrtc::VideoCodecMode::kScreensharing)
			? webrtc::VideoContentType::SCREENSHARE
			: webrtc::VideoContentType::UNSPECIFIED;
		encoded_images_[i].timing_.flags = webrtc::VideoSendTiming::kInvalid;
		encoded_images_[i]._frameType = ConvertToVideoFrameType(info.eFrameType);
		encoded_images_[i].SetSpatialIndex(configurations_[i].simulcast_idx);

		// Split encoded image up into fragments. This also updates
		// |encoded_image_|.
		RtpFragmentize(&encoded_images_[i], frame_packet);

		// Encoder can skip frames to save bandwidth in which case
		// |encoded_images_[i]._length| == 0.
		if (encoded_images_[i].size() > 0) {
			// Parse QP.
			h264_bitstream_parser_.ParseBitstream(encoded_images_[i]);
			encoded_images_[i].qp_ =
				h264_bitstream_parser_.GetLastSliceQp().value_or(-1);

			// Deliver encoded image.
			webrtc::CodecSpecificInfo codec_specific;
			codec_specific.codecType = webrtc::kVideoCodecH264;
			codec_specific.codecSpecific.H264.packetization_mode = packetization_mode_;
			codec_specific.codecSpecific.H264.temporal_idx = webrtc::kNoTemporalIdx;
			codec_specific.codecSpecific.H264.idr_frame = (info.eFrameType == videoFrameTypeIDR);
			codec_specific.codecSpecific.H264.base_layer_sync = false;

			encoded_image_callback_->OnEncodedImage(encoded_images_[i], &codec_specific);
		}
	}

	return WEBRTC_VIDEO_CODEC_OK;
}

void QsvEncoder::ReportInit()
{
	if (has_reported_init_)
		return;
	RTC_HISTOGRAM_ENUMERATION("WebRTC.Video.QsvEncoder.Event",
		kH264EncoderEventInit, kH264EncoderEventMax);
	has_reported_init_ = true;
}

void QsvEncoder::ReportError()
{
	if (has_reported_error_)
		return;
	RTC_HISTOGRAM_ENUMERATION("WebRTC.Video.QsvEncoder.Event",
		kH264EncoderEventError, kH264EncoderEventMax);
	has_reported_error_ = true;
}

webrtc::VideoEncoder::EncoderInfo QsvEncoder::GetEncoderInfo() const
{
	EncoderInfo info;
	info.supports_native_handle = false;
	info.implementation_name = "QsvEncoder";
	info.scaling_settings = VideoEncoder::ScalingSettings(kLowH264QpThreshold, kHighH264QpThreshold);
	info.is_hardware_accelerated = true;
	info.has_internal_source = false;
	info.supports_simulcast = true;
	info.preferred_pixel_formats = { webrtc::VideoFrameBuffer::Type::kI420 };
	return info;
}

void QsvEncoder::LayerConfig::SetStreamState(bool send_stream)
{
	if (send_stream && !sending) {
		// Need a key frame if we have not sent this stream before.
		key_frame_request = true;
	}
	sending = send_stream;
}

void i420ToNv12(unsigned char* src_i420_data, int width, int height, unsigned char* src_nv12_data)
{
	int src_y_size = width * height;
	int src_u_size = (width >> 1) * (height >> 1);

	unsigned char* src_nv12_y_data = src_nv12_data;
	unsigned char* src_nv12_uv_data = src_nv12_data + src_y_size;

	unsigned char* src_i420_y_data = src_i420_data;
	unsigned char* src_i420_u_data = src_i420_data + src_y_size;
	unsigned char* src_i420_v_data = src_i420_data + src_y_size + src_u_size;

	libyuv::I420ToNV12(
		(const uint8_t*)src_i420_y_data, width,
		(const uint8_t*)src_i420_u_data, width >> 1,
		(const uint8_t*)src_i420_v_data, width >> 1,
		(uint8_t*)src_nv12_y_data, width,
		(uint8_t*)src_nv12_uv_data, width,
		width, height);
}

bool QsvEncoder::EncodeFrame(int index, const webrtc::VideoFrame& input_frame,
	std::vector<uint8_t>& frame_packet)
{
	frame_packet.clear();

	if (qsv_encoders_.empty() || !qsv_encoders_[index]) {
		return false;
	}

	int width = input_frame.width();
	int height = input_frame.height();

	rtc::scoped_refptr<webrtc::I420BufferInterface> i420_buffer =
		input_frame.video_frame_buffer()->ToI420();
	int ret = libyuv::ConvertFromI420(
			i420_buffer->DataY(), i420_buffer->StrideY(), i420_buffer->DataU(),
			i420_buffer->StrideU(), i420_buffer->DataV(), i420_buffer->StrideV(),
			image_buffer_.get(), width, input_frame.width(), input_frame.height(),
			libyuv::FOURCC_NV12);
	if (ret < 0) {
		return false;
	}

	int image_size = width * height * 3 / 2; // nv12
	xop::IntelD3DEncoder* qsv_encoder = reinterpret_cast<xop::IntelD3DEncoder*>(qsv_encoders_[index]);
	if (qsv_encoder) {
		int frame_size = qsv_encoder->Encode(std::vector<uint8_t>(image_buffer_.get(), image_buffer_.get() + image_size), frame_packet);
		if (frame_size < 0) {
			return false;
		}
	}

	return true;
}

}  // namespace webrtc
