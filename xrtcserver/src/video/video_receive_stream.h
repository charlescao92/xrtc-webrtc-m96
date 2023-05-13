#ifndef VIDEO_VIDEO_RECEIVE_STREAM_H_
#define VIDEO_VIDEO_RECEIVE_STREAM_H_

#include <system_wrappers/include/clock.h>
#include <rtc_base/third_party/sigslot/sigslot.h>
#include <modules/rtp_rtcp/source/rtp_video_header.h>
#include <api/transport/rtp/dependency_descriptor.h>
#include <rtc_base/numerics/sequence_number_util.h>
#include <absl/types/optional.h>

#include "base/event_loop.h"
#include "video/video_stream_config.h"
#include "modules/rtp_rtcp/rtp_rtcp_impl.h"
#include "modules/rtp_rtcp/rtp_packet_to_send.h"
#include "modules/rtp_rtcp/include/receive_statistics.h"
#include "modules/video_coding/nack_requester.h"

namespace xrtc {

class VideoReceiveStream : public sigslot::has_slots<>
{
public:
    VideoReceiveStream(EventLoop* el, webrtc::Clock* clock, const VideoReceiveStreamConfig& config);
    ~VideoReceiveStream();

    void UpdateRtpStats(std::shared_ptr<RtpPacketToSend> packet,bool is_rtx, bool is_retransmit);
    void OnSendingRtpFrame(uint32_t rtp_timestamp, int64_t capture_time_ms, bool forced_report);
    void DeliverRtcp(const uint8_t* packet, size_t length);
    void DeliverRtp(const uint8_t* packet, size_t length);

    sigslot::signal1<const std::vector<uint16_t>&> SignalSendNack;

private:
enum ParseGenericDependenciesResult {
    kDropPacket,
    kHasGenericDescriptor,
    kNoGenericDescriptor
  };

    void OnNackSend(const std::vector<uint16_t>& seq_nums);
    ParseGenericDependenciesResult ParseGenericDependenciesExtension(
      const RtpPacketReceived& rtp_packet,
      webrtc::RTPVideoHeader* video_header);

private:
    EventLoop* el_;
    VideoReceiveStreamConfig config_;
    uint16_t rtx_seq_ = 1000;  // TODO 当前固定，应该是随机数的

    const std::unique_ptr<ReceiveStatistics> rtp_receive_statistics_;
    std::unique_ptr<ModuleRtpRtcpImpl> rtp_rtcp_;
    std::unique_ptr<NackRequester> nack_module_;

    std::unique_ptr<webrtc::FrameDependencyStructure> video_structure_;
    absl::optional<int64_t> video_structure_frame_id_;
    webrtc::SeqNumUnwrapper<uint16_t> frame_id_unwrapper_;
    uint32_t remote_ssrc_ = 0;
};

} // namespace xrtc

#endif // VIDEO_VIDEO_RECEIVE_STREAM_H_
