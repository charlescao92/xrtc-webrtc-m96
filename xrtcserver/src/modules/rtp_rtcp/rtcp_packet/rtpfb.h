#ifndef MODULES_RTP_RTCP_RTCP_PACKET_RTPFB_H_
#define MODULES_RTP_RTCP_RTCP_PACKET_RTPFB_H_

#include "modules/rtp_rtcp/rtcp_packet.h"

namespace xrtc {
namespace rtcp {

class Rtpfb : public RtcpPacket {
public:
    static const uint8_t kPacketType = 205;
    Rtpfb() = default;
    ~Rtpfb() override = default;

    void SetMediaSsrc(uint32_t ssrc) { media_ssrc_ = ssrc; }
    uint32_t media_ssrc() const { return media_ssrc_; }

protected:
    static constexpr size_t kCommonFeedbackLength = 8;

    void ParseCommonFeedback(const uint8_t* payload);
    void CreateCommonFeedback(uint8_t* payload) const;

private:
    uint32_t media_ssrc_ = 0;
};

} // namespace rtcp
} // namespace xrtc

#endif // MODULES_RTP_RTCP_RTCP_PACKET_RTPFB_H_
