#ifndef AUDIO_AUDIO_SEND_STREAM_H_
#define AUDIO_AUDIO_SEND_STREAM_H_

#include <system_wrappers/include/clock.h>

#include "base/event_loop.h"
#include "audio/audio_stream_config.h"
#include "modules/rtp_rtcp/rtp_rtcp_impl.h"

namespace xrtc {

class AudioSendStream {
public:
    AudioSendStream(EventLoop* el, webrtc::Clock* clock, const AudioSendStreamConfig& config);
    ~AudioSendStream();

    void UpdateRtpStats(std::shared_ptr<RtpPacketToSend> packet, bool is_retransmit);
    void OnSendingRtpFrame(uint32_t rtp_timestamp, int64_t capture_time_ms);
    
private:
    AudioSendStreamConfig config_;
    std::unique_ptr<ModuleRtpRtcpImpl> rtp_rtcp_;
};

} // namespace xrtc

#endif // XRTCSDK_XRTC_RTC_AUDIO_AUDIO_SEND_STREAM_H_