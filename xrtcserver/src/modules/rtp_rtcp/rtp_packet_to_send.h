#ifndef MODULES_RTP_RTCP_RTP_PACKET_TO_SEND_H_
#define MODULES_RTP_RTCP_RTP_PACKET_TO_SEND_H_

#include <absl/types/optional.h>

#include "modules/rtp_rtcp/rtp_packet.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"

namespace xrtc {

class RtpPacketToSend : public RtpPacket {
public:
    RtpPacketToSend();
    RtpPacketToSend(size_t capacity);

    void set_packet_type(RtpPacketMediaType type) {
        packet_type_ = type;
    }

    absl::optional<RtpPacketMediaType> packet_type() const {
        return packet_type_;
    }

private:
    absl::optional<RtpPacketMediaType> packet_type_;
};

} // end namespace xrtc

#endif // MODULES_RTP_RTCP_RTP_PACKET_TO_SEND_H_