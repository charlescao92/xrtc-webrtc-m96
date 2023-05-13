/**
 * @file rtp_utils.h
 * @author charles
 * @brief 
*/

#ifndef  __MODULE_RTP_UTILS_H_
#define  __MODULE_RTP_UTILS_H_

#include <api/array_view.h>
#include <system_wrappers/include/ntp_time.h>

namespace xrtc {

enum class RtpPacketType {
    k_rtp,
    k_rtcp,
    k_unknown,
};

RtpPacketType infer_rtp_packet_type(rtc::ArrayView<const char> packet);

uint16_t parse_rtp_sequence_number(rtc::ArrayView<const uint8_t> packet);
uint32_t parse_rtp_ssrc(rtc::ArrayView<const uint8_t> packet);
bool get_rtcp_type(const void* data, size_t len, int* type);

// 取整数部分的低 16 位，小数部分的高 16 位，压缩后的时间单位是 1/2^16 秒
inline uint32_t compact_ntp(webrtc::NtpTime ntp_time) {
    return (ntp_time.seconds()) << 16 | (ntp_time.fractions() >> 16);
}

// 压缩的NTP时间转换成ms
int64_t compact_ntp_rtt_to_ms(uint32_t compact_ntp_interval);

} // namespace xrtc

#endif  // __MODULE_RTP_UTILS_H_
