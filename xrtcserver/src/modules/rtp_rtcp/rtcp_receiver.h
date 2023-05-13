#ifndef MODULES_RTP_RTCP_RTCP_RECEIVER_H_
#define MODULES_RTP_RTCP_RTCP_RECEIVER_H_

#include <vector>
#include <stdint.h>
#include <stdlib.h>

#include <api/array_view.h>
#include <system_wrappers/include/ntp_time.h>
#include <api/units/time_delta.h>
#include <rtc_base/synchronization/mutex.h>
#include <rtc_base/thread_annotations.h>
#include <rtc_base/containers/flat_map.h>
#include <absl/types/optional.h>

#include "modules/rtp_rtcp/rtp_rtcp_interface.h"
#include "modules/rtp_rtcp/rtcp_packet/common_header.h"
#include "modules/rtp_rtcp/rtcp_packet/receiver_report.h"

namespace xrtc {

class RTCPReceiver {
public:
    RTCPReceiver(const RtpRtcpInterface::Configuration& config);
    ~RTCPReceiver();

    void IncomingRtcpPacket(rtc::ArrayView<const uint8_t> packet);

    bool NTP(uint32_t* received_ntp_secs,
           uint32_t* received_ntp_frac,
           uint32_t* rtcp_arrival_time_secs,
           uint32_t* rtcp_arrival_time_frac,
           uint32_t* rtcp_timestamp,
           uint32_t* remote_sender_packet_count,
           uint64_t* remote_sender_octet_count,
           uint64_t* remote_sender_reports_count) const;

private:
    class PacketInformation;

    bool ParseCompoundPacket(rtc::ArrayView<const uint8_t> packet,
        PacketInformation& packet_info);
    void HandleReceiverReport(const rtcp::CommonHeader& rtcp_block,
        PacketInformation& packet_info);
    void HandleSenderReport(const rtcp::CommonHeader& rtcp_block,
        PacketInformation& packet_info);
    void HandleReportBlock(const rtcp::ReportBlock& report_block,
        uint32_t remote_ssrc, PacketInformation& packet_info);
    void HandleNack(const rtcp::CommonHeader& rtcp_block,
        PacketInformation& packet_info);
    bool IsRegisteredSsrc(uint32_t ssrc);

private:
    webrtc::Clock* clock_;
    bool audio_;
    RtpRtcpModuleObserver* rtp_rtcp_module_observer_;
    uint32_t num_skipped_packets_ = 0;
    std::vector<uint32_t> registered_ssrcs_;

    // 最后一次收到RR包的时间
    webrtc::Timestamp last_received_rb_ = webrtc::Timestamp::PlusInfinity();

    uint32_t remote_ssrc_ = 0;

    // Received sender report.
    webrtc::NtpTime remote_sender_ntp_time_ ;
    uint32_t remote_sender_rtp_time_ ;
    // When did we receive the last send report.
    webrtc::NtpTime last_received_sr_ntp_ ;
    uint32_t remote_sender_packet_count_ ;
    uint64_t remote_sender_octet_count_ ;
    uint64_t remote_sender_reports_count_;

};

} // namespace xrtc
#endif // MODULES_RTP_RTCP_RTCP_RECEIVER_H_
