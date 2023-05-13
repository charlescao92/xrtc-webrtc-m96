#ifndef MODULES_RTP_RTCP_RTCP_PACKET_SENDER_REPORT_H_
#define MODULES_RTP_RTCP_RTCP_PACKET_SENDER_REPORT_H_

#include <vector>

#include <system_wrappers/include/ntp_time.h>

#include "modules/rtp_rtcp/rtcp_packet.h"
#include "modules/rtp_rtcp/rtcp_packet/report_block.h"
#include "modules/rtp_rtcp/rtcp_packet/common_header.h"

namespace xrtc {
    namespace rtcp {

        class SenderReport : public RtcpPacket {
        public:
            static const uint8_t kPacketType = 200;

            SenderReport() = default;
            ~SenderReport() override = default;

            bool Parse(const CommonHeader& packet);

            void SetNtpTime(webrtc::NtpTime ntp_time) {
                ntp_time_ = ntp_time;
            }

            webrtc::NtpTime ntp_time() { return ntp_time_; }
            uint32_t rtp_timestamp() const { return rtp_timestamp_; }
            uint32_t sender_packet_count() const { return sender_packet_count_; }
            uint32_t sender_octet_count() const { return sender_octet_count_; }

            void SetRtpTimestamp(uint32_t rtp_timestamp) {
                rtp_timestamp_ = rtp_timestamp;
            }
            uint32_t rtp_timestamp() { return rtp_timestamp_; }

            void SetSendPacketCount(uint32_t packet_count) {
                sender_packet_count_ = packet_count;
            }

            void SetSendPacketOctet(uint32_t packet_octet) {
                sender_octet_count_ = packet_octet;
            }

            size_t BlockLength() const override;
            bool Create(uint8_t* packet,
                size_t* index,
                size_t max_length,
                PacketReadyCallback callback) const override;

            const std::vector<ReportBlock>& report_blocks() const {
                return report_blocks_;
            }

        private:
            static const size_t kSenderBaseLength = 24;

            webrtc::NtpTime ntp_time_;
            uint32_t rtp_timestamp_;
            uint32_t sender_packet_count_;
            uint32_t sender_octet_count_;
            std::vector<ReportBlock> report_blocks_;
        };

    } // namespace rtcp

} // namespace xrtc

#endif // MODULES_RTP_RTCP_RTCP_PACKET_SENDER_REPORT_H_