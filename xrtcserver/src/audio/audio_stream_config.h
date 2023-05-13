#ifndef AUDIO_AUDIO_STREAM_CONFIG_H_
#define AUDIO_AUDIO_STREAM_CONFIG_H_

#include <stdint.h>

namespace xrtc {

class RtpRtcpModuleObserver;

struct AudioSendStreamConfig {
    struct Rtp {
        uint32_t ssrc = 0;
        int payload_type = -1;
        int clock_rate = 48000;
    } rtp;

    // 音频的rtcp包发送间隔
    int rtcp_report_interval_ms = 5000;
    RtpRtcpModuleObserver* rtp_rtcp_module_observer = nullptr;
};

struct AudioReceiveStreamConfig {
    struct Rtp {
        uint32_t remote_ssrc = 0;
        uint32_t local_ssrc = 0;
        int payload_type = -1;
        int clock_rate = 48000;
    } rtp;

    // 音频的rtcp包发送间隔
    int rtcp_report_interval_ms = 5000;
    RtpRtcpModuleObserver* rtp_rtcp_module_observer = nullptr;
};

} // namespace xrtc

#endif // AUDIO_AUDIO_STREAM_CONFIG_H_
