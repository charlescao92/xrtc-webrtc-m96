#include <modules/rtp_rtcp/source/byte_io.h>
#include <rtc_base/logging.h>

#include "modules/rtp_rtcp/rtcp_packet/sender_report.h"

namespace xrtc {
namespace rtcp {

    // RTCP receiver report (RFC 3550).
    //
    //   0                   1                   2                   3
    //   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //  |V=2|P|    RC   |   PT=SR=200   |             length            |
    //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //  |                     SSRC of packet sender                     |
    //  +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
    //  |                         report block(s)                       |
    //  |                            ....                               |
    bool SenderReport::Parse(const CommonHeader& packet) {
        const uint8_t report_block_count = packet.count();
        if (packet.payload_size() < kSenderBaseLength + report_block_count * ReportBlock::kLength) {
            RTC_LOG(LS_WARNING) << "rr payload_size is not enough, payload_size: "
                    << packet.payload_size() << ", report_block_count: " << report_block_count;
            return false;
        }

        const uint8_t* const payload = packet.payload();
        SetSenderSsrc(webrtc::ByteReader<uint32_t>::ReadBigEndian(&payload[0]));
        uint32_t secs = webrtc::ByteReader<uint32_t>::ReadBigEndian(&payload[4]);
        uint32_t frac = webrtc::ByteReader<uint32_t>::ReadBigEndian(&payload[8]);
        ntp_time_.Set(secs, frac);
        rtp_timestamp_ = webrtc::ByteReader<uint32_t>::ReadBigEndian(&payload[12]);
        sender_packet_count_ = webrtc::ByteReader<uint32_t>::ReadBigEndian(&payload[16]);
        sender_packet_count_ = webrtc::ByteReader<uint32_t>::ReadBigEndian(&payload[20]);
        report_blocks_.resize(report_block_count);

        const uint8_t* next_block = payload + kSenderBaseLength;
        for (ReportBlock& block : report_blocks_) {
            bool block_parsed = block.Parse(next_block, ReportBlock::kLength);
            RTC_DCHECK(block_parsed);
            next_block += ReportBlock::kLength;
        }

        // 检查读取位置是否越界了
        RTC_DCHECK_LE(next_block - payload, static_cast<ptrdiff_t>(packet.payload_size()));

        return true;
    }

    size_t SenderReport::BlockLength() const {
        return kHeaderSize + kSenderBaseLength +
            report_blocks_.size() * ReportBlock::kLength;
    }

    bool SenderReport::Create(uint8_t* packet,
        size_t* index, 
        size_t max_length, 
        PacketReadyCallback callback) const
    {
        while (*index + BlockLength() > max_length) { // 容量已经不够了
            // 如果是第一个包，容量就不足，直接return false
            // 如果是非第一个包，容量不足，可以先将之前已经构建好的包，先回调到上层，此时index清0
            if (!OnBufferFull(packet, index, callback)) {
                return false;
            }
        }

        const size_t index_end = *index + BlockLength();

        // 创建头部
        CreateHeader(report_blocks_.size(), kPacketType, HeaderLength(), packet, index);
        // 创建Sender Report固定的部分
        webrtc::ByteWriter<uint32_t>::WriteBigEndian(&packet[*index + 0], sender_ssrc());
        webrtc::ByteWriter<uint32_t>::WriteBigEndian(&packet[*index + 4], ntp_time_.seconds());
        webrtc::ByteWriter<uint32_t>::WriteBigEndian(&packet[*index + 8], ntp_time_.fractions());
        webrtc::ByteWriter<uint32_t>::WriteBigEndian(&packet[*index + 12], rtp_timestamp_);
        webrtc::ByteWriter<uint32_t>::WriteBigEndian(&packet[*index + 16], sender_packet_count_);
        webrtc::ByteWriter<uint32_t>::WriteBigEndian(&packet[*index + 20], sender_packet_count_);

        *index += kSenderBaseLength;

        // Write report blocks.
        for (const ReportBlock& block : report_blocks_) {
            block.Create(packet + *index);
            *index += ReportBlock::kLength;
        }
            
        // Ensure bytes written match expected.
        RTC_DCHECK_EQ(*index, index_end);

        return true;
    }

} // namespace rtcp

} // namespace xrtc
