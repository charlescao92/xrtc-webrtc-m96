#ifndef VIDEO_VIDEO_STREAM_CONFIG_H_
#define VIDEO_VIDEO_STREAM_CONFIG_H_

#include <stdint.h>

namespace xrtc {

class RtpRtcpModuleObserver;

struct VideoSendStreamConfig {
    struct Rtp {
        uint32_t ssrc = 0;
        int payload_type = -1;
        int clock_rate = 90000;

        struct Rtx {
            uint32_t ssrc = 0;
            int payload_type = -1;
        } rtx;

    } rtp;

    // 视频的rtcp包发送间隔
    int rtcp_report_interval_ms = 1000;
    RtpRtcpModuleObserver* rtp_rtcp_module_observer = nullptr;
};


struct VideoReceiveStreamConfig {
    struct Rtp {
        uint32_t remote_ssrc = 0;
        int payload_type = -1;
        int clock_rate = 90000;

        uint32_t local_ssrc = 0;

        struct Rtx {
            uint32_t ssrc = 0;
            int payload_type = -1;
        } rtx;

    } rtp;

    // 视频的rtcp包发送间隔
    int rtcp_report_interval_ms = 1000;
    RtpRtcpModuleObserver* rtp_rtcp_module_observer = nullptr;
};

} // end namespace xrtc

#endif // VIDEO_VIDEO_STREAM_CONFIG_H_
