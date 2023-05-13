#include "nv_encoder.h"
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

namespace xrtc {

	NvEncoder::NvEncoder(const cricket::VideoCodec& codec)
		: packetization_mode_(webrtc::H264PacketizationMode::SingleNalUnit),
		max_payload_size_(0),
		number_of_cores_(0),
		encoded_image_callback_(nullptr),
		has_reported_init_(false),
		has_reported_error_(false),
		num_temporal_layers_(1),
		tl0sync_limit_(0)
	{
		RTC_CHECK(absl::EqualsIgnoreCase(codec.name, cricket::kH264CodecName));
		std::string packetization_mode_string;
		if (codec.GetParam(cricket::kH264FmtpPacketizationMode, &packetization_mode_string)
			&& packetization_mode_string == "1") {
			packetization_mode_ = webrtc::H264PacketizationMode::NonInterleaved;
		}

		encoded_images_.reserve(webrtc::kMaxSimulcastStreams);
		nv_encoders_.reserve(webrtc::kMaxSimulcastStreams);
		configurations_.reserve(webrtc::kMaxSimulcastStreams);
		image_buffer_ = nullptr;
	}

	NvEncoder::~NvEncoder()
	{
		Release();
	}

	int32_t NvEncoder::InitEncode(const webrtc::VideoCodec* inst,
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
		nv_encoders_.resize(number_of_streams);
		configurations_.resize(number_of_streams);

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
			xop::NvidiaD3D11Encoder* nv_encoder = new xop::NvidiaD3D11Encoder();
			nv_encoders_[i] = nv_encoder;

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

			nv_encoder->SetOption(xop::VE_OPT_WIDTH, configurations_[i].width);
			nv_encoder->SetOption(xop::VE_OPT_HEIGHT, configurations_[i].height);
			nv_encoder->SetOption(xop::VE_OPT_FRAME_RATE, static_cast<int>(configurations_[i].max_frame_rate));
			nv_encoder->SetOption(xop::VE_OPT_GOP, configurations_[i].key_frame_interval);
			nv_encoder->SetOption(xop::VE_OPT_CODEC, xop::VE_OPT_CODEC_H264);
			nv_encoder->SetOption(xop::VE_OPT_BITRATE_KBPS, configurations_[i].target_bps / 1000);
			nv_encoder->SetOption(xop::VE_OPT_TEXTURE_FORMAT, xop::VE_OPT_FORMAT_B8G8R8A8);
			if (!nv_encoder->Init()) {
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
		}

		webrtc::SimulcastRateAllocator init_allocator(codec_);
		webrtc::VideoBitrateAllocation allocation = init_allocator.GetAllocation(
			codec_.maxBitrate * 1000 / 2, codec_.maxFramerate);
		SetRates(RateControlParameters(allocation, codec_.maxFramerate));
		return WEBRTC_VIDEO_CODEC_OK;
	}

	int32_t NvEncoder::Release()
	{
		while (!nv_encoders_.empty()) {
			xop::NvidiaD3D11Encoder* nv_encoder = reinterpret_cast<xop::NvidiaD3D11Encoder*>(nv_encoders_.back());
			if (nv_encoder) {
				nv_encoder->Destroy();
				delete nv_encoder;
			}
			nv_encoders_.pop_back();
		}

		configurations_.clear();
		encoded_images_.clear();

		return WEBRTC_VIDEO_CODEC_OK;
	}

	int32_t NvEncoder::RegisterEncodeCompleteCallback(webrtc::EncodedImageCallback* callback)
	{
		encoded_image_callback_ = callback;
		return WEBRTC_VIDEO_CODEC_OK;
	}

	void NvEncoder::SetRates(const RateControlParameters& parameters)
	{
		if (parameters.bitrate.get_sum_bps() == 0) {
			// Encoder paused, turn off all encoding.
			for (size_t i = 0; i < configurations_.size(); ++i)
				configurations_[i].SetStreamState(false);
			return;
		}

		// At this point, bitrate allocation should already match codec settings.
		if (codec_.maxBitrate > 0)
			RTC_DCHECK_LE(parameters.bitrate.get_sum_kbps(), codec_.maxBitrate);
		RTC_DCHECK_GE(parameters.bitrate.get_sum_kbps(), codec_.minBitrate);
		if (codec_.numberOfSimulcastStreams > 0)
			RTC_DCHECK_GE(parameters.bitrate.get_sum_kbps(), codec_.simulcastStream[0].minBitrate);

		codec_.maxFramerate = static_cast<uint32_t>(parameters.framerate_fps);

		size_t stream_idx = nv_encoders_.size() - 1;
		for (size_t i = 0; i < nv_encoders_.size(); ++i, --stream_idx) {
			configurations_[i].target_bps = parameters.bitrate.GetSpatialLayerSum(stream_idx);
			configurations_[i].max_frame_rate = static_cast<float>(parameters.framerate_fps);

			if (configurations_[i].target_bps) {
				configurations_[i].SetStreamState(true);

				if (nv_encoders_[i]) {
					xop::NvidiaD3D11Encoder* nv_encoder = reinterpret_cast<xop::NvidiaD3D11Encoder*>(nv_encoders_[i]);
					nv_encoder->SetEvent(xop::VE_EVENT_RESET_BITRATE_KBPS, configurations_[i].target_bps / 1000);
					nv_encoder->SetEvent(xop::VE_EVENT_RESET_FRAME_RATE, static_cast<int>(configurations_[i].max_frame_rate));
				}
				else {
					configurations_[i].SetStreamState(false);
				}
			}
		}
	}

	int32_t NvEncoder::Encode(const webrtc::VideoFrame& input_frame,
		const std::vector<webrtc::VideoFrameType>* frame_types)
	{
		if (nv_encoders_.empty()) {
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

		rtc::scoped_refptr<const webrtc::I420BufferInterface> frame_buffer = input_frame.video_frame_buffer()->ToI420();
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
			for (size_t i = 0; i < frame_types->size() && i < configurations_.size(); ++i) {
				if ((*frame_types)[i] == webrtc::VideoFrameType::kVideoFrameKey && configurations_[i].sending) {
					send_key_frame = true;
					break;
				}
			}
		}

		RTC_DCHECK_EQ(configurations_[0].width, frame_buffer->width());
		RTC_DCHECK_EQ(configurations_[0].height, frame_buffer->height());

		// Encode image for each layer.
		for (size_t i = 0; i < nv_encoders_.size(); ++i) {
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
				if (!nv_encoders_.empty() && nv_encoders_[i]) {
					xop::NvidiaD3D11Encoder* nv_encoder = reinterpret_cast<xop::NvidiaD3D11Encoder*>(nv_encoders_[i]);
					nv_encoder->SetEvent(xop::VE_EVENT_FORCE_IDR, 1);
				}

				configurations_[i].key_frame_request = false;
			}

			// EncodeFrame output.
			SFrameBSInfo info;
			memset(&info, 0, sizeof(SFrameBSInfo));
			std::vector<uint8_t> frame_packet;

			bool success = EncodeFrame((int)i, input_frame, frame_packet);
			if (!success) {
				return WEBRTC_VIDEO_CODEC_ERROR;
			}

			if (frame_packet.size() == 0) {
				return WEBRTC_VIDEO_CODEC_OK;
			}
			else {
				if ((frame_packet[4] & 0x1f) == 0x07) {
					info.eFrameType = videoFrameTypeIDR;
				}
				else if ((frame_packet[4] & 0x1f) == 0x01) {
					info.eFrameType = videoFrameTypeP;
				}
				else {
					return WEBRTC_VIDEO_CODEC_OK;
				}
			}

			encoded_images_[i]._encodedWidth = configurations_[i].width;
			encoded_images_[i]._encodedHeight = configurations_[i].height;
			encoded_images_[i].SetTimestamp(input_frame.timestamp());
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
				codec_specific.codecSpecific.H264.idr_frame = info.eFrameType == videoFrameTypeIDR;
				codec_specific.codecSpecific.H264.base_layer_sync = false;

				encoded_image_callback_->OnEncodedImage(encoded_images_[i], &codec_specific);
			}
		}

		return WEBRTC_VIDEO_CODEC_OK;
	}

	void NvEncoder::ReportInit()
	{
		if (has_reported_init_)
			return;
		RTC_HISTOGRAM_ENUMERATION("WebRTC.Video.NvEncoder.Event",
			kH264EncoderEventInit, kH264EncoderEventMax);
		has_reported_init_ = true;
	}

	void NvEncoder::ReportError()
	{
		if (has_reported_error_)
			return;
		RTC_HISTOGRAM_ENUMERATION("WebRTC.Video.NvEncoder.Event",
			kH264EncoderEventError, kH264EncoderEventMax);
		has_reported_error_ = true;
	}

	webrtc::VideoEncoder::EncoderInfo NvEncoder::GetEncoderInfo() const
	{
		EncoderInfo info;
		info.supports_native_handle = false;
		info.implementation_name = "NvEncoder";
		info.scaling_settings = VideoEncoder::ScalingSettings(kLowH264QpThreshold, kHighH264QpThreshold);
		info.is_hardware_accelerated = true;
		info.has_internal_source = false;
		info.supports_simulcast = true;
		info.preferred_pixel_formats = { webrtc::VideoFrameBuffer::Type::kI420 };
		return info;
	}

	void NvEncoder::LayerConfig::SetStreamState(bool send_stream)
	{
		if (send_stream && !sending) {
			// Need a key frame if we have not sent this stream before.
			key_frame_request = true;
		}
		sending = send_stream;
	}

	bool NvEncoder::EncodeFrame(int index, const webrtc::VideoFrame& input_frame,
		std::vector<uint8_t>& frame_packet)
	{
		frame_packet.clear();

		if (nv_encoders_.empty() || !nv_encoders_[index]) {
			return false;
		}

		if (video_format_ == EVideoFormatType::videoFormatI420) {
			if (image_buffer_ != nullptr) {
				if (webrtc::ConvertFromI420(input_frame, webrtc::VideoType::kARGB, 0,
					image_buffer_.get()) < 0) {
					return false;
				}
			}
			else {
				return false;
			}
		}

		int width = input_frame.width();
		int height = input_frame.height();
		int image_size = width * height * 4; // argb

		xop::NvidiaD3D11Encoder* nv_encoder = reinterpret_cast<xop::NvidiaD3D11Encoder*>(nv_encoders_[index]);
		if (nv_encoder) {
			int frame_size = nv_encoder->Encode(std::vector<uint8_t>(image_buffer_.get(), image_buffer_.get() + image_size), frame_packet);
			if (frame_size < 0) {
				return false;
			}
		}

		return true;
	}

}  // namespace webrtc
