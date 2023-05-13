#include "modules/rtp_rtcp/rtcp_packet/nack.h"

#include <algorithm>
#include <cstdint>
#include <utility>

#include <rtc_base/checks.h>
#include <rtc_base/logging.h>
#include <modules/rtp_rtcp/source/byte_io.h>

#include "modules/rtp_rtcp/rtcp_packet/common_header.h"

namespace xrtc {
namespace rtcp {

constexpr uint8_t Nack::kFeedbackMessageType;
constexpr size_t Nack::kNackItemLength;

size_t Nack::BlockLength() const {
    return kHeaderSize + kCommonFeedbackLength + packed_.size() * kNackItemLength;
}

bool Nack::Create(uint8_t* packet,
                  size_t* index,
                  size_t max_length,
                  PacketReadyCallback callback) const {
  RTC_DCHECK(!packed_.empty());
  // If nack list can't fit in packet, try to fragment.
  constexpr size_t kNackHeaderLength = kHeaderSize + kCommonFeedbackLength; // 4 + 8
  for (size_t nack_index = 0; nack_index < packed_.size();) {
    size_t bytes_left_in_buffer = max_length - *index;
    if (bytes_left_in_buffer < kNackHeaderLength + kNackItemLength) {
      if (!OnBufferFull(packet, index, callback))
        return false;
      continue;
    }
    size_t num_nack_fields =
        std::min((bytes_left_in_buffer - kNackHeaderLength) / kNackItemLength,
                 packed_.size() - nack_index);

    size_t payload_size_bytes =
        kCommonFeedbackLength + (num_nack_fields * kNackItemLength);
    size_t payload_size_32bits =
        rtc::CheckedDivExact<size_t>(payload_size_bytes, 4);
    CreateHeader(kFeedbackMessageType, kPacketType, payload_size_32bits, packet,
                 index);

    CreateCommonFeedback(packet + *index);
    *index += kCommonFeedbackLength;

    size_t nack_end_index = nack_index + num_nack_fields;
    for (; nack_index < nack_end_index; ++nack_index) {
        const PackedNack& item = packed_[nack_index];
        webrtc::ByteWriter<uint16_t>::WriteBigEndian(packet + *index + 0, item.first_pid);
        webrtc::ByteWriter<uint16_t>::WriteBigEndian(packet + *index + 2, item.bitmask);
        *index += kNackItemLength;
    }
    RTC_DCHECK_LE(*index, max_length);
  }

  return true;
}

bool Nack::Parse(const rtcp::CommonHeader& packet) {
    // 1、先判断长度是否足够
    if (packet.payload_size() < kCommonFeedbackLength + kNackItemLength) {
        RTC_LOG(LS_WARNING) << "payload length " << packet.payload_size()
            << " is too small for nack";
        return false;
    }

    // 2、解析Sender ssrc和media source ssrc
    ParseCommonFeedback(packet.payload());

    // 3、解析FCI
    size_t nack_items = (packet.payload_size() - kCommonFeedbackLength) / kNackItemLength;
    packet_ids_.clear();
    packed_.resize(nack_items);

    const uint8_t* next_nack = packet.payload() + kCommonFeedbackLength;
    for (size_t i = 0; i < nack_items; ++i) {
        packed_[i].first_pid = webrtc::ByteReader<uint16_t>::ReadBigEndian(next_nack);
        packed_[i].bitmask = webrtc::ByteReader<uint16_t>::ReadBigEndian(next_nack + 2);
        next_nack += kNackItemLength;
    }

    // 解包获取丢包的id
    Unpack();

    return true;
}

void Nack::SetPacketIds(const uint16_t* nack_list, size_t length) {
  RTC_DCHECK(nack_list);
  SetPacketIds(std::vector<uint16_t>(nack_list, nack_list + length));
}

void Nack::SetPacketIds(std::vector<uint16_t> nack_list) {
  RTC_DCHECK(packet_ids_.empty());
  RTC_DCHECK(packed_.empty());
  packet_ids_ = std::move(nack_list);
  Pack();
}

void Nack::Pack() {
    RTC_DCHECK(!packet_ids_.empty());
    RTC_DCHECK(packed_.empty());
    auto it = packet_ids_.begin();
    const auto end = packet_ids_.end();
    while (it != end) {
        PackedNack item;
        item.first_pid = *it++;
        // Bitmask specifies losses in any of the 16 packets following the pid.
        item.bitmask = 0;
        while (it != end) {
            uint16_t shift = static_cast<uint16_t>(*it - item.first_pid - 1);
            if (shift <= 15) {
                item.bitmask |= (1 << shift);
                ++it;
            } else {
                break;
            }
        }
        packed_.push_back(item);
    }
}

void Nack::Unpack() {
    for (const PackedNack& nack : packed_) {
        packet_ids_.push_back(nack.first_pid);
        // 循环判断bitmask
        uint16_t pid = nack.first_pid + 1;
        // 每次不断的右移，获得nack_item，再判断是否是1，是1则代表丢包，保存到丢包集合packet_ids_
        for (uint16_t nack_item = nack.bitmask; nack_item != 0; nack_item >>= 1, ++pid) {
            if (nack_item & 1) {
                packet_ids_.push_back(pid);
            }
        }
    }
}

} // namespace rtcp
} // namespace xrtc