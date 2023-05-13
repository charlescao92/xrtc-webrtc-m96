#ifndef MODULES_RTP_RTCP_RTCP_PACKET_REPORT_BLOCK_H_
#define MODULES_RTP_RTCP_RTCP_PACKET_REPORT_BLOCK_H_

#include <stdint.h>
#include <stdlib.h>

namespace xrtc {
namespace rtcp {

class ReportBlock {
public:
    static const size_t kLength = 24;
    ReportBlock() = default;
    ~ReportBlock() = default;

    bool Parse(const uint8_t* buffer, size_t len);

    // Fills buffer with the ReportBlock.
    // Consumes ReportBlock::kLength bytes.
    void Create(uint8_t* buffer) const;

    void SetMediaSsrc(uint32_t ssrc) { source_ssrc_ = ssrc; }
    void SetFractionLost(uint8_t fraction_lost) {
        fraction_lost_ = fraction_lost;
    }

    bool SetCumulativeLost(int32_t cumulative_lost);
    void SetExtHighestSeqNum(uint32_t ext_highest_seq_num) {
        extended_high_seq_num_ = ext_highest_seq_num;
    }
    void SetJitter(uint32_t jitter) { jitter_ = jitter; }
    void SetLastSr(uint32_t last_sr) { last_sr_ = last_sr; }
    void SetDelayLastSr(uint32_t delay_last_sr) {
        delay_since_last_sr_ = delay_last_sr;
    }

    uint32_t source_ssrc() const { return source_ssrc_; }
    uint32_t last_sr() const { return last_sr_; }
    uint32_t delay_since_last_sr() const { return delay_since_last_sr_; }
    int32_t packets_lost() const { return cumulative_packets_lost_; }
    uint32_t cumulative_lost() const;
    uint8_t fraction_lost() const { return fraction_lost_; }
    uint32_t jitter() const { return jitter_; }
    uint32_t extended_high_seq_num() const { return extended_high_seq_num_; }
            
private:
    uint32_t source_ssrc_ = 0;
    uint8_t fraction_lost_ = 0;
    int32_t cumulative_packets_lost_ = 0;
    uint32_t extended_high_seq_num_ = 0;
    uint32_t jitter_ = 0;
    uint32_t last_sr_ = 0;
    uint32_t delay_since_last_sr_ = 0;
};

} // namespace rtcp
} // namespace xrtc

#endif // MODULES_RTP_RTCP_RTCP_PACKET_REPORT_BLOCK_H_
