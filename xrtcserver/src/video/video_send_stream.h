#ifndef VIDEO_VIDEO_SEND_STREAM_H_
#define VIDEO_VIDEO_SEND_STREAM_H_

#include <system_wrappers/include/clock.h>

#include "base/event_loop.h"
#include "video/video_stream_config.h"
#include "modules/rtp_rtcp/rtp_rtcp_impl.h"
#include "modules/rtp_rtcp/rtp_packet_to_send.h"

namespace xrtc {

class VideoSendStream {
public:
    VideoSendStream(EventLoop* el, webrtc::Clock* clock, const VideoSendStreamConfig& config);
    ~VideoSendStream();

    void UpdateRtpStats(std::shared_ptr<RtpPacketToSend> packet,bool is_rtx, bool is_retransmit);
    void OnSendingRtpFrame(uint32_t rtp_timestamp, int64_t capture_time_ms, bool forced_report);
    void DeliverRtcp(const uint8_t* packet, size_t length);
    std::unique_ptr<RtpPacketToSend> BuildRtxPacket(std::shared_ptr<RtpPacketToSend> packet);

private:
    VideoSendStreamConfig config_;
    std::unique_ptr<ModuleRtpRtcpImpl> rtp_rtcp_;
    uint16_t rtx_seq_ = 1000;  // TODO 当前固定，应该是随机数的
};

} // namespace xrtc

#endif // VIDEO_VIDEO_SEND_STREAM_H_
