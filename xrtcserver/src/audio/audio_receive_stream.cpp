#include "audio/audio_receive_stream.h"
#include "modules/rtp_rtcp/include/receive_statistics_impl.h"
#include "modules/rtp_rtcp/include/rtp_packet_received.h"

namespace xrtc {

    std::unique_ptr<ModuleRtpRtcpImpl> CreateRtpRtcpModule(EventLoop* el, webrtc::Clock* clock,
        const AudioReceiveStreamConfig& asconfig, ReceiveStatistics* rtp_receive_statistics) 
    {
        RtpRtcpInterface::Configuration config;
        config.audio = true;
        config.receiver_only = false;
        config.clock = clock;
        config.local_ssrc = asconfig.rtp.local_ssrc;
        config.remote_ssrc = asconfig.rtp.remote_ssrc;
        config.payload_type = asconfig.rtp.payload_type;
        config.rtcp_report_interval_ms = asconfig.rtcp_report_interval_ms;
        config.clock_rate = asconfig.rtp.clock_rate;
        config.rtp_rtcp_module_observer = asconfig.rtp_rtcp_module_observer;
        config.receive_statistics = rtp_receive_statistics;

        auto rtp_rtcp = std::make_unique<ModuleRtpRtcpImpl>(el, config);
        return std::move(rtp_rtcp);
    }

    AudioReceiveStream::AudioReceiveStream(EventLoop* el, webrtc::Clock* clock,
        const AudioReceiveStreamConfig& config) :
        el_(el),
        config_(config),
        rtp_receive_statistics_(ReceiveStatistics::Create(clock)),
        rtp_rtcp_(CreateRtpRtcpModule(el, clock, config, rtp_receive_statistics_.get()))
    {
        rtp_receive_statistics_->SetMaxReorderingThreshold(config_.rtp.remote_ssrc,
                                        kDefaultMaxReorderingThreshold);
                                        
        // 设置RTCP包为复合包模式，同时启动定时器
        rtp_rtcp_->SetRTCPStatus(webrtc::RtcpMode::kCompound);
        rtp_rtcp_->SetSendingStatus(false);
    }

    AudioReceiveStream::~AudioReceiveStream() {
    }

    void AudioReceiveStream::UpdateRtpStats(std::shared_ptr<RtpPacketToSend> packet,
        bool is_retransmit)
    {
        rtp_rtcp_->UpdateRtpStats(packet, false, is_retransmit);
    }

    void AudioReceiveStream::OnSendingRtpFrame(uint32_t rtp_timestamp,
        int64_t capture_time_ms)
    {
        rtp_rtcp_->OnSendingRtpFrame(rtp_timestamp, capture_time_ms, false);
    }

     void AudioReceiveStream::DeliverRtcp(const uint8_t* packet, size_t length) {
        rtp_rtcp_->IncomingRtcpPacket(packet, length);
    }

    void AudioReceiveStream::DeliverRtp(const uint8_t* packet, size_t length) {
        RtpPacketReceived rtp_packet_received;
        rtp_packet_received.Parse(packet, length);
        rtp_receive_statistics_->OnRtpPacket(rtp_packet_received);
    }

} // namespace xrtc