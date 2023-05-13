#include "video/video_send_stream.h"

#include <modules/rtp_rtcp/source/byte_io.h>


namespace xrtc {

    const uint16_t kRtxHeaderSize = 2;

    std::unique_ptr<ModuleRtpRtcpImpl> CreateRtpRtcpModule(EventLoop* el, webrtc::Clock* clock,
        const VideoSendStreamConfig& vsconfig)
    {
        RtpRtcpInterface::Configuration config;
        config.audio = false;
        config.receiver_only = false;
        config.clock = clock;
        config.local_ssrc = vsconfig.rtp.ssrc;
        config.payload_type = vsconfig.rtp.payload_type;
        config.rtcp_report_interval_ms = vsconfig.rtcp_report_interval_ms;
        config.clock_rate = vsconfig.rtp.clock_rate;
        config.rtp_rtcp_module_observer = vsconfig.rtp_rtcp_module_observer;

        auto rtp_rtcp = std::make_unique<ModuleRtpRtcpImpl>(el, config);
        return std::move(rtp_rtcp);
    }

	VideoSendStream::VideoSendStream(EventLoop* el, webrtc::Clock* clock,
		const VideoSendStreamConfig& config) :
		config_(config),
        rtp_rtcp_(CreateRtpRtcpModule(el, clock, config))
	{
        rtp_rtcp_->SetRTCPStatus(webrtc::RtcpMode::kCompound);
        rtp_rtcp_->SetSendingStatus(true);
	}

	VideoSendStream::~VideoSendStream()
	{
	}

    void VideoSendStream::UpdateRtpStats(std::shared_ptr<RtpPacketToSend> packet, 
        bool is_rtx, bool is_retransmit) 
    {
        rtp_rtcp_->UpdateRtpStats(packet, is_rtx, is_retransmit);
    }

    void VideoSendStream::OnSendingRtpFrame(uint32_t rtp_timestamp, 
        int64_t capture_time_ms, 
        bool forced_report)
    {
        rtp_rtcp_->OnSendingRtpFrame(rtp_timestamp, capture_time_ms, forced_report);
    }

    void VideoSendStream::DeliverRtcp(const uint8_t* packet, size_t length) {
        rtp_rtcp_->IncomingRtcpPacket(packet, length);
    }

    std::unique_ptr<RtpPacketToSend> VideoSendStream::BuildRtxPacket(
        std::shared_ptr<RtpPacketToSend> packet)
    {
        auto rtx_packet = std::make_unique<RtpPacketToSend>();
        rtx_packet->SetPayloadType(config_.rtp.rtx.payload_type);
        rtx_packet->SetSsrc(config_.rtp.rtx.ssrc);
        rtx_packet->SetSequenceNumber(rtx_seq_++);
        rtx_packet->SetMarker(packet->marker());
        rtx_packet->SetTimestamp(packet->timestamp());

        // 分配负载的内存
        auto rtx_payload = rtx_packet->AllocatePayload(packet->payload_size()
            + kRtxHeaderSize);
        if (!rtx_payload) {
            return nullptr;
        }

        // 写入原始的sequence_number
        webrtc::ByteWriter<uint16_t>::WriteBigEndian(rtx_payload, packet->sequence_number());
        
        // 写入原始的负载数据
        auto payload = packet->payload();
        memcpy(rtx_payload + kRtxHeaderSize, payload.data(), payload.size());

        return rtx_packet;
    }

} // namespace xrtc