#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/rtp_packet.h"

namespace xrtc {

    RtpPacketCounter::RtpPacketCounter(const RtpPacket& packet) :
        header_bytes(packet.header_size()),
        payload_bytes(packet.payload_size()),
        padding_bytes(packet.padding_size()),
        packets(1)
    {
    }

    void RtpPacketCounter::Add(const RtpPacketCounter& other) {
        header_bytes += other.header_bytes;
        payload_bytes += other.payload_bytes;
        padding_bytes += other.padding_bytes;
        packets += other.packets;
    }

    void RtpPacketCounter::Subtract(const RtpPacketCounter& other) {
        header_bytes -= other.header_bytes;
        payload_bytes -= other.payload_bytes;
        padding_bytes -= other.padding_bytes;
        packets -= other.packets;
    }

    void RtpPacketCounter::AddPacket(const RtpPacket& packet) {
        header_bytes += packet.header_size();
        payload_bytes += packet.payload_size();
        padding_bytes += packet.padding_size();
        ++packets;
    }

    StreamDataCounters::StreamDataCounters() : first_packet_time_ms(-1) {}


} // namespace xrtc