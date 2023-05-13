#ifndef MODULES_RTP_RTCP_RTCP_PACKET_NACK_H_
#define MODULES_RTP_RTCP_RTCP_PACKET_NACK_H_

#include <vector>

#include "modules/rtp_rtcp/rtcp_packet/rtpfb.h"

namespace xrtc {
namespace rtcp {

class CommonHeader;

class Nack : public Rtpfb {
public:
    static const uint8_t kFeedbackMessageType = 1;

    Nack() = default;
    ~Nack() override = default;

    size_t BlockLength() const override;
    bool Create(uint8_t* packet,
        size_t* index,
        size_t max_length,
        PacketReadyCallback callback) const override;
    bool Parse(const rtcp::CommonHeader& packet);

    void SetPacketIds(const uint16_t* nack_list, size_t length);
    void SetPacketIds(std::vector<uint16_t> nack_list);
    
    const std::vector<uint16_t>& packet_ids() const { return packet_ids_; }

private:
    void Pack();    // Fills packed_ using packed_ids_. (used in SetPacketIds).
    void Unpack();  // Fills packet_ids_ using packed_. (used in Parse).

private:
    static const size_t kNackItemLength = 4;

    // FCI的结构
    struct PackedNack {
        uint16_t first_pid; // PID: 表示丢包的起始参考值
        uint16_t bitmask;   // BLP: 表示丢包的位图
    };
   
    std::vector<PackedNack> packed_;
    std::vector<uint16_t> packet_ids_;
};

} // namespace rtcp
} // namespace xrtc

#endif // MODULES_RTP_RTCP_RTCP_PACKET_NACK_H_