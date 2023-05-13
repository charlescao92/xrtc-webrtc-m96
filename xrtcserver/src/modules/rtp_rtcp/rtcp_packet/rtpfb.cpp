#include "modules/rtp_rtcp/rtcp_packet/rtpfb.h"

#include "modules/rtp_rtcp/source/byte_io.h"

namespace xrtc {
namespace rtcp {

void Rtpfb::ParseCommonFeedback(const uint8_t* payload) {
    SetSenderSsrc(webrtc::ByteReader<uint32_t>::ReadBigEndian(payload));
    SetMediaSsrc(webrtc::ByteReader<uint32_t>::ReadBigEndian(payload + 4));
}

void Rtpfb::CreateCommonFeedback(uint8_t* payload) const {
    webrtc::ByteWriter<uint32_t>::WriteBigEndian(&payload[0], sender_ssrc());
    webrtc::ByteWriter<uint32_t>::WriteBigEndian(&payload[4], media_ssrc());
}

} // namespace rtcp
} // namespace xrtc