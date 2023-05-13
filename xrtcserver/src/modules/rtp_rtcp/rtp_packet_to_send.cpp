#include "modules/rtp_rtcp/rtp_packet_to_send.h"

namespace xrtc {

RtpPacketToSend::RtpPacketToSend() : RtpPacket() { 
}

RtpPacketToSend::RtpPacketToSend(size_t capacity) : RtpPacket(capacity) {
  
}

} // end namespace xrtc
