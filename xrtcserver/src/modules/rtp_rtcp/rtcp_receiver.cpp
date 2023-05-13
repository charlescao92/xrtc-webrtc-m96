#include <rtc_base/logging.h>

#include "modules/rtp_rtcp/rtcp_receiver.h"
#include "modules/rtp_rtcp/rtcp_packet/receiver_report.h"
#include "modules/rtp_rtcp/rtcp_packet/sender_report.h"
#include "modules/rtp_rtcp/rtcp_packet/nack.h"
#include "modules/rtp_rtcp/rtp_utils.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"

namespace xrtc {

namespace {

// 3秒钟触发超时
const int kRrTimeoutIntervals = 3;

constexpr webrtc::TimeDelta kDefaultVideoReportInterval = webrtc::TimeDelta::Seconds(1);
constexpr webrtc::TimeDelta kDefaultAudioReportInterval = webrtc::TimeDelta::Seconds(5);

}

struct RTCPReceiver::PacketInformation {
	uint32_t packet_type_flags = 0;  // RTCPPacketTypeFlags bit field.

  	uint32_t remote_ssrc = 0;
  	std::vector<uint16_t> nack_sequence_numbers;

  	int64_t rtt_ms = 0;
  	uint32_t receiver_estimated_max_bitrate_bps = 0;
};


RTCPReceiver::RTCPReceiver(const RtpRtcpInterface::Configuration& config) :
				clock_(config.clock),
				audio_(config.audio),
				rtp_rtcp_module_observer_(config.rtp_rtcp_module_observer),
				remote_ssrc_(config.remote_ssrc),
      			remote_sender_rtp_time_(0),
      			remote_sender_packet_count_(0),
      		remote_sender_octet_count_(0),
      		remote_sender_reports_count_(0),
			num_skipped_packets_(0)
{
	registered_ssrcs_.push_back(config.local_ssrc);
}

RTCPReceiver::~RTCPReceiver() {
}

void RTCPReceiver::IncomingRtcpPacket(rtc::ArrayView<const uint8_t> packet) {
	if (packet.empty()) {
		RTC_LOG(LS_WARNING) << "rtcp packet is empty";
		return;
	}

	PacketInformation packet_info;
	if (!ParseCompoundPacket(packet, packet_info)) {
		return;
	}
}

bool RTCPReceiver::ParseCompoundPacket(rtc::ArrayView<const uint8_t> packet, 
	PacketInformation& packet_info)
{
	rtcp::CommonHeader rtcp_block;
	for (const uint8_t* next_block = packet.begin();
		next_block != packet.end();
		next_block = rtcp_block.NextPacket())
	{
		ptrdiff_t reamining_packet_size = packet.end() - next_block;
		if (!rtcp_block.Parse(next_block, reamining_packet_size)) {
			if (next_block == packet.begin()) {
				RTC_LOG(LS_WARNING) << "parse rtcp packet failed";
				return false;
			}

			// 如果不是第一个包解析出错，则记录一下
			++num_skipped_packets_;
			break;
		}

		switch (rtcp_block.packet_type()) {
			case rtcp::SenderReport::kPacketType: // SR 200
				HandleSenderReport(rtcp_block, packet_info);
				break;
			case RTCPPayloadType::kRtpFb: // 205
				switch (rtcp_block.fmt()) {
				case rtcp::Nack::kFeedbackMessageType: // 1 NACK
					HandleNack(rtcp_block, packet_info);
					break;
				default:
					++num_skipped_packets_;
					break;
				}
				break;
			default:
				RTC_LOG(LS_WARNING) << "rtcp packet not handle, packet_type: " <<
					(int)(rtcp_block.packet_type());
				break;
		}

	}
	return false;
}

void RTCPReceiver::HandleSenderReport(const rtcp::CommonHeader& rtcp_block, 
	PacketInformation& packet_info)
{
	rtcp::SenderReport sender_report;
	if (!sender_report.Parse(rtcp_block)) {
		++num_skipped_packets_;
		return;
	}

	uint32_t remote_ssrc = sender_report.sender_ssrc();

	packet_info.remote_ssrc = remote_ssrc;

	if (remote_ssrc_ == 0) {
		remote_ssrc_ = remote_ssrc;
	}

  	if (remote_ssrc_ == remote_ssrc) {
    	//收到了SR包，保存SR包的参数
    	packet_info.packet_type_flags |= RTCPPacketType::kRtcpSr;

    	remote_sender_ntp_time_ = sender_report.ntp_time();
    	remote_sender_rtp_time_ = sender_report.rtp_timestamp();
    	last_received_sr_ntp_ = clock_->CurrentNtpTime();
    	remote_sender_packet_count_ = sender_report.sender_packet_count();
    	remote_sender_octet_count_ = sender_report.sender_octet_count();
    	remote_sender_reports_count_++;

  	} else {
		// 保存所有的report_block
    	packet_info.packet_type_flags |= RTCPPacketType::kRtcpRr;
  	}

	for (const rtcp::ReportBlock& report_block : sender_report.report_blocks()) {
		HandleReportBlock(report_block, remote_ssrc, packet_info);
	}

}

void RTCPReceiver::HandleReceiverReport(const rtcp::CommonHeader& rtcp_block, 
		PacketInformation& packet_info)
{
	rtcp::ReceiverReport rr;
	if (!rr.Parse(rtcp_block)) {
		++num_skipped_packets_;
		return;
	}

	uint32_t remote_ssrc = rr.sender_ssrc();

	packet_info.remote_ssrc = remote_ssrc;
	packet_info.packet_type_flags |= RTCPPacketType::kRtcpRr;

	for (const rtcp::ReportBlock& report_block : rr.report_blocks()) {
		HandleReportBlock(report_block, remote_ssrc, packet_info);
	}

}

void RTCPReceiver::HandleReportBlock(const rtcp::ReportBlock& report_block, 
	uint32_t remote_ssrc, 
	PacketInformation& packet_info)
{
	if (!IsRegisteredSsrc(report_block.source_ssrc())) {
		return;
	}

	last_received_rb_ = clock_->CurrentTime();

	// 计算rtt的值
	int64_t rtt_ms = 0;
	uint32_t send_ntp_time = report_block.last_sr();
	if (send_ntp_time != 0) {
		uint32_t delay_ntp = report_block.delay_since_last_sr();
		// 压缩64位时间，返回32位的ntp时间
		uint32_t receive_ntp_time =
				compact_ntp(clock_->ConvertTimestampToNtpTime(last_received_rb_));
		uint32_t rtt_ntp = receive_ntp_time - send_ntp_time - delay_ntp;
		// 需要将rtt_ntp转换成ms
		rtt_ms = compact_ntp_rtt_to_ms(rtt_ntp);

		// 传给上一级
		if (rtp_rtcp_module_observer_) {
				rtp_rtcp_module_observer_->OnNetworkInfo(rtt_ms,
					report_block.packets_lost(),
					report_block.fraction_lost(),
					report_block.jitter());
		}

	}
}

void RTCPReceiver::HandleNack(const rtcp::CommonHeader& rtcp_block, 
		PacketInformation& packet_info)
	{
		rtcp::Nack nack;
		if (!nack.Parse(rtcp_block)) {
			++num_skipped_packets_;
			return;
		}

		if (rtp_rtcp_module_observer_) {
			rtp_rtcp_module_observer_->OnNackReceived(
				audio_ ? webrtc::MediaType::AUDIO : webrtc::MediaType::VIDEO,
				nack.packet_ids());
		}
	}

bool RTCPReceiver::IsRegisteredSsrc(uint32_t ssrc) {
	for (auto rssrc : registered_ssrcs_) {
		if (rssrc == ssrc) {
			return true;
		}
	}
	return false;
}

bool RTCPReceiver::NTP(uint32_t* received_ntp_secs,
                       uint32_t* received_ntp_frac,
                       uint32_t* rtcp_arrival_time_secs,
                       uint32_t* rtcp_arrival_time_frac,
                       uint32_t* rtcp_timestamp,
                       uint32_t* remote_sender_packet_count,
                       uint64_t* remote_sender_octet_count,
                       uint64_t* remote_sender_reports_count) const 
{
  if (!last_received_sr_ntp_.Valid())
    return false;

  // NTP from incoming SenderReport.
  if (received_ntp_secs)
    *received_ntp_secs = remote_sender_ntp_time_.seconds();
  if (received_ntp_frac)
    *received_ntp_frac = remote_sender_ntp_time_.fractions();
  // Rtp time from incoming SenderReport.
  if (rtcp_timestamp)
    *rtcp_timestamp = remote_sender_rtp_time_;

  // Local NTP time when we received a RTCP packet with a send block.
  if (rtcp_arrival_time_secs)
    *rtcp_arrival_time_secs = last_received_sr_ntp_.seconds();
  if (rtcp_arrival_time_frac)
    *rtcp_arrival_time_frac = last_received_sr_ntp_.fractions();

  // Counters.
  if (remote_sender_packet_count)
    *remote_sender_packet_count = remote_sender_packet_count_;
  if (remote_sender_octet_count)
    *remote_sender_octet_count = remote_sender_octet_count_;
  if (remote_sender_reports_count)
    *remote_sender_reports_count = remote_sender_reports_count_;

  return true;
}

} // namespace xrtc