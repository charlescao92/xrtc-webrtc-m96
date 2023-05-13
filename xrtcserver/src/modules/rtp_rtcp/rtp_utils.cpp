#include <rtc_base/byte_io.h>
#include <rtc_base/numerics/divide_round.h>

#include "modules/rtp_rtcp/rtp_utils.h"

namespace xrtc {

const uint8_t k_rtp_version = 2;
const size_t k_min_rtp_packet_len = 12;
const size_t k_min_rtcp_packet_len = 4;

// 第一个字节前两位是版本号，右移六位
bool has_correct_rtp_version(rtc::ArrayView<const uint8_t> packet) {
    return packet[0] >> 6 == k_rtp_version;
}

bool payload_type_is_reserved_for_rtcp(uint8_t payload_type) {
    return 64 <= payload_type && payload_type < 96;
}

bool is_rtp_packet(rtc::ArrayView<const uint8_t> packet) {
    return packet.size() >= k_min_rtp_packet_len &&
        has_correct_rtp_version(packet) &&
        !payload_type_is_reserved_for_rtcp(packet[1] & 0x7F);
}

bool is_rtcp_packet(rtc::ArrayView<const uint8_t> packet) {
    return packet.size() >= k_min_rtcp_packet_len &&
        has_correct_rtp_version(packet) &&
        payload_type_is_reserved_for_rtcp(packet[1] & 0x7F);
}

RtpPacketType infer_rtp_packet_type(rtc::ArrayView<const char> packet) { 
    if (is_rtp_packet(rtc::reinterpret_array_view<const uint8_t>(packet))) {
        return RtpPacketType::k_rtp;
    }
    
    if (is_rtcp_packet(rtc::reinterpret_array_view<const uint8_t>(packet))) {
        return RtpPacketType::k_rtcp;
    }
 
    return RtpPacketType::k_unknown;
}

uint16_t parse_rtp_sequence_number(rtc::ArrayView<const uint8_t> packet) {
    return rtc::ByteReader<uint16_t>::ReadBigEndian(packet.data() + 2); // 从第二个字节开始读sequence_number
}

uint32_t parse_rtp_ssrc(rtc::ArrayView<const uint8_t> packet) {
    return rtc::ByteReader<uint32_t>::ReadBigEndian(packet.data() + 8); // 从第八个字节开始读ssrc
}

bool get_rtcp_type(const void* data, size_t len, int* type) {
    if (len < k_min_rtcp_packet_len) {
        return false;
    }

    if (!data || !type) {
        return false;
    }

    *type = *((const uint8_t*)data + 1);
    return true;
}

int64_t compact_ntp_rtt_to_ms(uint32_t compact_ntp_interval) {
        if (compact_ntp_interval > 0x80000000) {
            return 1;
        }
        // Convert to 64bit value to avoid multiplication overflow.
        int64_t value = static_cast<int64_t>(compact_ntp_interval);
        // To convert to milliseconds need to divide by 2^16 to get seconds,
        // then multiply by 1000 to get milliseconds. To avoid float operations,
        // multiplication and division swapped.
        int64_t ms = webrtc::DivideRoundToNearest(value * 1000, 1 << 16);
        // Rtt value 0 considered too good to be true and increases to 1.
        return std::max<int64_t>(ms, 1);
    }

} // namespace xrtc


