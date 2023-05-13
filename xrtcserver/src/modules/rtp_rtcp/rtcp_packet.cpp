#include "modules/rtp_rtcp/rtcp_packet.h"

namespace xrtc {
namespace rtcp {

void RtcpPacket::CreateHeader(size_t count_or_fmt,
        uint8_t packet_type, 
        uint16_t block_length, 
        uint8_t* buffer, 
        size_t* pos)
{
    CreateHeader(count_or_fmt, packet_type, block_length, false, buffer, pos);
}

void RtcpPacket::CreateHeader(size_t count_or_fmt, 
        uint8_t packet_type, 
        uint16_t block_length,
        bool padding,
        uint8_t* buffer, 
        size_t* pos)
{
    const uint8_t kVersionBits = 2 << 6;
    uint8_t padding_bit = padding ? (1 << 5) : 0;
    buffer[*pos + 0] = kVersionBits | padding_bit | static_cast<uint8_t>(count_or_fmt);
    buffer[*pos + 1] = packet_type;
    buffer[*pos + 2] = (block_length >> 8) & 0xFF;
    buffer[*pos + 3] = block_length & 0xFF;
    *pos += kHeaderSize;
}

bool RtcpPacket::OnBufferFull(
        uint8_t* packet, 
        size_t* index, 
        PacketReadyCallback callback) const
{
    if (*index == 0) {
        // 第一个需要复合的包，已经容量不足，此种情况是无法打包的
        return false;
    }

    // 如果不是第一个包，先将已经复合的包回调给上层，同时我们需要新启用一个复合包
    callback(rtc::ArrayView<const uint8_t>(packet, *index));

    // 启用一个新的复合包
    *index = 0;
    
    return true;
}

size_t RtcpPacket::HeaderLength() const {
    return (BlockLength() - kHeaderSize) / 4;
}

} // namespace rtcp
} // namespace xrtc
