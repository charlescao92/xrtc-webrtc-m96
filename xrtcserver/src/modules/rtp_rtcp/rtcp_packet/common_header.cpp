#include "modules/rtp_rtcp/rtcp_packet/common_header.h"

#include <rtc_base/logging.h>
#include <modules/rtp_rtcp/source/byte_io.h>

namespace xrtc {
namespace rtcp {
CommonHeader::CommonHeader() {
}

CommonHeader::~CommonHeader() {
}

// From RFC 3550, RTP: A Transport Protocol for Real-Time Applications.
//
// RTP header format.
//   0                   1                   2                   3
//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |V=2|P| RC/FMT  |      PT       |             length            |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
bool CommonHeader::Parse(const uint8_t* buffer, size_t len) {
    const uint8_t kRtpVersion = 2;
    if (len < kHeaderSizeBytes) {
        RTC_LOG(LS_WARNING) << "invalid rtcp packet, buffer is not enough, len: " << len;
        return false;
    }

    // 判断版本是否正确
    uint8_t version = buffer[0] >> 6;
    if (version != kRtpVersion) {
        RTC_LOG(LS_WARNING) << "invalid rtcp packet, version is not " << kRtpVersion;
        return false;
    }

    bool has_padding = (buffer[0] & 0x20) == 1;
    count_or_fmt_ = buffer[0] & 0x1F;
    packet_type_ = buffer[1];
    payload_size_ = webrtc::ByteReader<uint16_t>::ReadBigEndian(&buffer[2]) * 4;
    padding_size_ = 0;
    payload_ = buffer + kHeaderSizeBytes;

    if (len < payload_size_ + kHeaderSizeBytes) {
        RTC_LOG(LS_WARNING) << "invalid rtcp packet, buffer is not enough, "
            << "payload_size: " << payload_size_;
        return false;
    }

    if (has_padding) {
        if (payload_size_ == 0) {
            RTC_LOG(LS_WARNING) << "invalid rtcp packet, has padding, but payload_size is 0";
            return false;
        }

        // 最后一个字节表示padding的大小
        padding_size_ = payload_[payload_size_ - 1];
        if (padding_size_ == 0) {
            RTC_LOG(LS_WARNING) << "invalid rtcp packet, has padding, but padding_size is 0";
            return false;
        }

        // payload_size真实的大小是要减去padding_size
        payload_size_ -= padding_size_;
    }
    
    return true;
}
} // namespace rtcp

} // namespace xrtc
