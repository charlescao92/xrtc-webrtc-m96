#ifndef MODULES_RTP_RTCP_RTCP_PACKET_RECEIVER_REPORT_H_
#define MODULES_RTP_RTCP_RTCP_PACKET_RECEIVER_REPORT_H_

#include <vector>

#include "modules/rtp_rtcp/rtcp_packet.h"
#include "modules/rtp_rtcp/rtcp_packet/report_block.h"
#include "modules/rtp_rtcp/rtcp_packet/common_header.h"

namespace xrtc {
    namespace rtcp {

        class ReceiverReport : public RtcpPacket {
        public:
            static const uint8_t kPacketType = 201;
            static constexpr size_t kMaxNumberOfReportBlocks = 0x1f;

            ReceiverReport() = default;
            ~ReceiverReport() override = default;

            size_t BlockLength() const override;

            bool Create(uint8_t* packet,
                size_t* index,
                size_t max_length,
                PacketReadyCallback callback) const override;

            bool AddReportBlock(const ReportBlock& block);
            bool SetReportBlocks(std::vector<ReportBlock> blocks);

            bool Parse(const CommonHeader& packet);

            const std::vector<ReportBlock>& report_blocks() const {
                return report_blocks_;
            }

        private:
            static const size_t kRrBaseLength = 4;
            std::vector<ReportBlock> report_blocks_;
        };

    } // namespace rtcp
} // namespace xrtc

#endif // MODULES_RTP_RTCP_RTCP_PACKET_RECEIVER_REPORT_H_