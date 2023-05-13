#ifndef AUDIO_AUDIO_RECEIVE_STREAM_H_
#define AUDIO_AUDIO_RECEIVE_STREAM_H_

#include <system_wrappers/include/clock.h>

#include "base/event_loop.h"
#include "audio/audio_stream_config.h"
#include "modules/rtp_rtcp/rtp_rtcp_impl.h"
#include "modules/rtp_rtcp/include/receive_statistics.h"
#include "modules/rtp_rtcp/include/rtp_packet_received.h"

namespace xrtc {

class AudioReceiveStream {
public:
    AudioReceiveStream(EventLoop* el, webrtc::Clock* clock, const AudioReceiveStreamConfig& config);
    ~AudioReceiveStream();

    void UpdateRtpStats(std::shared_ptr<RtpPacketToSend> packet, bool is_retransmit);
    void OnSendingRtpFrame(uint32_t rtp_timestamp, int64_t capture_time_ms);
    void DeliverRtcp(const uint8_t* packet, size_t length);
    void DeliverRtp(const uint8_t* packet, size_t length);

private:
    EventLoop* el_;
    AudioReceiveStreamConfig config_;
    const std::unique_ptr<ReceiveStatistics> rtp_receive_statistics_;
    std::unique_ptr<ModuleRtpRtcpImpl> rtp_rtcp_;
};

} // namespace xrtc

#endif // AUDIO_AUDIO_RECEIVE_STREAM_H_