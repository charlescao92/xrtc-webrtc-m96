#include "video/video_receive_stream.h"
#include "video/video_stream_config.h"
#include "modules/rtp_rtcp/include/receive_statistics_impl.h"
#include "modules/rtp_rtcp/include/rtp_packet_received.h"

#include <rtc_base/logging.h>
#include <modules/rtp_rtcp/source/byte_io.h>

#include "modules/rtp_rtcp/include/rtp_dependency_descriptor_extension.h"
#include "modules/rtp_rtcp/include/rtp_generic_frame_descriptor_extension.h"

namespace xrtc {

const uint16_t kRtxHeaderSize = 2;

std::unique_ptr<ModuleRtpRtcpImpl> CreateRtpRtcpModule(EventLoop* el, webrtc::Clock* clock,
    const VideoReceiveStreamConfig& vsconfig, ReceiveStatistics* rtp_receive_statistics)
{
    RtpRtcpInterface::Configuration config;
    config.audio = false;
    config.receiver_only = false;
    config.clock = clock;
    config.local_ssrc = vsconfig.rtp.local_ssrc;
    config.remote_ssrc = vsconfig.rtp.remote_ssrc;
    config.payload_type = vsconfig.rtp.payload_type;
    config.rtcp_report_interval_ms = vsconfig.rtcp_report_interval_ms;
    config.clock_rate = vsconfig.rtp.clock_rate;
    config.rtp_rtcp_module_observer = vsconfig.rtp_rtcp_module_observer;
    config.receive_statistics = rtp_receive_statistics;
    auto rtp_rtcp = std::make_unique<ModuleRtpRtcpImpl>(el, config);
    return std::move(rtp_rtcp);
}

VideoReceiveStream::VideoReceiveStream(EventLoop* el, webrtc::Clock* clock, 
	const VideoReceiveStreamConfig& config) :
	config_(config),
    rtp_receive_statistics_(ReceiveStatistics::Create(clock)),
    rtp_rtcp_(CreateRtpRtcpModule(el, clock, config, rtp_receive_statistics_.get())),
    nack_module_(std::make_unique<NackRequester>(el, clock))
{
    nack_module_->SignalSendNack.connect(this, &VideoReceiveStream::OnNackSend);

    if (config_.rtp.rtx.ssrc == 0) {
        rtp_receive_statistics_->EnableRetransmitDetection(config.rtp.remote_ssrc, true);
    }

    remote_ssrc_ = config_.rtp.remote_ssrc;

    rtp_receive_statistics_->SetMaxReorderingThreshold(config_.rtp.remote_ssrc, kDefaultMaxReorderingThreshold);
    rtp_rtcp_->SetRTCPStatus(webrtc::RtcpMode::kCompound);
    rtp_rtcp_->SetSendingStatus(false);
}

VideoReceiveStream::~VideoReceiveStream()
{
}

void VideoReceiveStream::UpdateRtpStats(std::shared_ptr<RtpPacketToSend> packet, 
    bool is_rtx, bool is_retransmit) 
{
    rtp_rtcp_->UpdateRtpStats(packet, is_rtx, is_retransmit);
}

void VideoReceiveStream::OnSendingRtpFrame(uint32_t rtp_timestamp, 
    int64_t capture_time_ms, 
    bool forced_report)
{
    rtp_rtcp_->OnSendingRtpFrame(rtp_timestamp, capture_time_ms, forced_report);
}

void VideoReceiveStream::DeliverRtcp(const uint8_t* packet, size_t length) {
    rtp_rtcp_->IncomingRtcpPacket(packet, length);
}

void VideoReceiveStream::DeliverRtp(const uint8_t* packet, size_t length) {
    RtpPacketReceived rtp_packet;
    rtp_packet.Parse(packet, length);

    rtp_receive_statistics_->OnRtpPacket(rtp_packet);

    // NACK检查
    webrtc::RTPVideoHeader video_header;
    ParseGenericDependenciesResult generic_descriptor_state =
            ParseGenericDependenciesExtension(rtp_packet, &video_header);
    if (generic_descriptor_state == kDropPacket)
        return;

    //  如果不判断remote_ssrc，则也会收到rtx的包
    if (nack_module_ && remote_ssrc_ == rtp_packet.ssrc()) { 
        const bool is_keyframe = video_header.is_first_packet_in_frame &&
                                video_header.frame_type == webrtc::VideoFrameType::kVideoFrameKey;
        nack_module_->OnReceivedPacket(rtp_packet.sequence_number(), is_keyframe, rtp_packet.recovered());    
    }
 
}

VideoReceiveStream::ParseGenericDependenciesResult
VideoReceiveStream::ParseGenericDependenciesExtension(
    const RtpPacketReceived& rtp_packet, webrtc::RTPVideoHeader* video_header) 
{
    if (rtp_packet.HasExtension<RtpDependencyDescriptorExtension>()) {
        webrtc::DependencyDescriptor dependency_descriptor;
        if (!rtp_packet.GetExtension<RtpDependencyDescriptorExtension>(
            video_structure_.get(), &dependency_descriptor)) {
            // Descriptor is there, but failed to parse. Either it is invalid,
            // or too old packet (after relevant video_structure_ changed),
            // or too new packet (before relevant video_structure_ arrived).
            // Drop such packet to be on the safe side.
            // TODO(bugs.webrtc.org/10342): Stash too new packet.
            RTC_LOG(LS_WARNING) << "ssrc: " << rtp_packet.ssrc()
                                << " Failed to parse dependency descriptor.";
            return kDropPacket;
        }

        if (dependency_descriptor.attached_structure != nullptr &&
            !dependency_descriptor.first_packet_in_frame) {
            RTC_LOG(LS_WARNING) << "ssrc: " << rtp_packet.ssrc()
                          << "Invalid dependency descriptor: structure "
                             "attached to non first packet of a frame.";
            return kDropPacket;
        }

        video_header->is_first_packet_in_frame = dependency_descriptor.first_packet_in_frame;
        video_header->is_last_packet_in_frame = dependency_descriptor.last_packet_in_frame;

        int64_t frame_id = frame_id_unwrapper_.Unwrap(dependency_descriptor.frame_number);
        auto& generic_descriptor_info = video_header->generic.emplace();
        generic_descriptor_info.frame_id = frame_id;
        generic_descriptor_info.spatial_index = dependency_descriptor.frame_dependencies.spatial_id;
        generic_descriptor_info.temporal_index = dependency_descriptor.frame_dependencies.temporal_id;
        for (int fdiff : dependency_descriptor.frame_dependencies.frame_diffs) {
            generic_descriptor_info.dependencies.push_back(frame_id - fdiff);
        }
        generic_descriptor_info.decode_target_indications =
            dependency_descriptor.frame_dependencies.decode_target_indications;
        if (dependency_descriptor.resolution) {
            video_header->width = dependency_descriptor.resolution->Width();
            video_header->height = dependency_descriptor.resolution->Height();
        }

        // FrameDependencyStructure is sent in dependency descriptor of the first
        // packet of a key frame and required for parsed dependency descriptor in
        // all the following packets until next key frame.
        // Save it if there is a (potentially) new structure.
        if (dependency_descriptor.attached_structure) {
          RTC_DCHECK(dependency_descriptor.first_packet_in_frame);
          if (video_structure_frame_id_ > frame_id) {
            RTC_LOG(LS_WARNING)
                << "Arrived key frame with id " << frame_id << " and structure id "
                << dependency_descriptor.attached_structure->structure_id
                << " is older than the latest received key frame with id "
                << *video_structure_frame_id_ << " and structure id "
                << video_structure_->structure_id;
            return kDropPacket;
          }
          video_structure_ = std::move(dependency_descriptor.attached_structure);
          video_structure_frame_id_ = frame_id;
          video_header->frame_type = webrtc::VideoFrameType::kVideoFrameKey;

        } else {
            video_header->frame_type = webrtc::VideoFrameType::kVideoFrameDelta;
        }

        return kHasGenericDescriptor;
    }

    webrtc::RtpGenericFrameDescriptor generic_frame_descriptor;
    if (!rtp_packet.GetExtension<RtpGenericFrameDescriptorExtension00>(
          &generic_frame_descriptor)) {
        return kNoGenericDescriptor;
    }

    video_header->is_first_packet_in_frame = generic_frame_descriptor.FirstPacketInSubFrame();
    video_header->is_last_packet_in_frame = generic_frame_descriptor.LastPacketInSubFrame();

    if (generic_frame_descriptor.FirstPacketInSubFrame()) {
        video_header->frame_type =
            generic_frame_descriptor.FrameDependenciesDiffs().empty()
                ? webrtc::VideoFrameType::kVideoFrameKey : webrtc::VideoFrameType::kVideoFrameDelta;

        auto& generic_descriptor_info = video_header->generic.emplace();
        int64_t frame_id = frame_id_unwrapper_.Unwrap(generic_frame_descriptor.FrameId());
        generic_descriptor_info.frame_id = frame_id;
        generic_descriptor_info.spatial_index = generic_frame_descriptor.SpatialLayer();
        generic_descriptor_info.temporal_index = generic_frame_descriptor.TemporalLayer();
        for (uint16_t fdiff : generic_frame_descriptor.FrameDependenciesDiffs()) {
            generic_descriptor_info.dependencies.push_back(frame_id - fdiff);
        }
    }

    video_header->width = generic_frame_descriptor.Width();
    video_header->height = generic_frame_descriptor.Height();

    return kHasGenericDescriptor;
}


void VideoReceiveStream::OnNackSend(const std::vector<uint16_t>& seq_nums) {
    rtp_rtcp_->SendNack(seq_nums);
}
 
} // end namespace xrtc