#ifndef MODULES_RTP_RTCP_RTCP_SENDER_H_
#define MODULES_RTP_RTCP_RTCP_SENDER_H_

#include <functional>
#include <set>
#include <map>

#include <stdlib.h>

#include <api/units/time_delta.h>
#include <api/rtp_headers.h>
#include <rtc_base/random.h>

#include "modules/rtp_rtcp/rtp_rtcp_interface.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/rtcp_packet/common_header.h"
#include "modules/rtp_rtcp/rtcp_packet/receiver_report.h"
#include "modules/rtp_rtcp/include/receive_statistics.h"
#include "modules/rtp_rtcp/rtcp_nack_stats.h"

namespace xrtc {

class RTCPReceiver;

class RTCPSender {
public:
	class PacketInformation;

	struct FeedbackState {
    	FeedbackState() = default;
    	FeedbackState(const FeedbackState&) = default;
    	FeedbackState(FeedbackState&&) = default;

    	~FeedbackState() = default;

    	uint32_t packets_sent = 0;
    	size_t media_bytes_sent = 0;
    	uint32_t send_bitrate = 0;

    	uint32_t last_rr_ntp_secs = 0;
    	uint32_t last_rr_ntp_frac = 0;
    	uint32_t remote_sr = 0;

    	// Used when generating TMMBR.
    	RTCPReceiver* receiver = nullptr;
	};

	RTCPSender(const RtpRtcpInterface::Configuration& config,
			std::function<void(webrtc::TimeDelta)> schedule_next_rtcp_send);
	~RTCPSender();

	bool Sending() const { return sending_; }

	void SetLastRtpTimestamp(uint32_t rtp_timestamp,
		absl::optional<webrtc::Timestamp> last_frame_capture_time);

	void SetRTCPStatus(webrtc::RtcpMode mode);
	void SetSendingStatus(bool sending) {
		sending_ = sending;
	}

	bool TimeToSendRTCPPacket(bool send_before_keyframe = false);
	int SendRTCP(const FeedbackState& feedback_state,
		RTCPPacketType packet_type,
		size_t nack_size = 0,
		const uint16_t* nack_list = 0);

private:
	class RtcpContext;
	class PacketSender;

		struct ReportFlag {
			ReportFlag(uint32_t type, bool is_volatile) :
				type(type), is_volatile(is_volatile) {}
			bool operator<(const ReportFlag& flag) const {
				return type < flag.type;
			}

			bool operator==(const ReportFlag& flag) const {
				return type == flag.type;
			}

			uint32_t type;
			bool is_volatile;
		};

		void SetNextRtcpSendEvaluationDuration(webrtc::TimeDelta duration);
		int ComputeCompoundRTCPPacket(const FeedbackState& feedback_state,
			RTCPPacketType packet_type,
			size_t nack_size,
			const uint16_t* nack_list,
			PacketSender& sender);
		void SetFlag(uint32_t type);
		bool ConsumeFlag(uint32_t type, bool forced = false);
		bool AllVolatileFlagsConsumed();
		void PrepareReport(const FeedbackState& feedback_state);

		void BuildSR(const RtcpContext& context, PacketSender& sender);
		void BuildRR(const RtcpContext& context, PacketSender& sender);
		void BuildNACK(const RtcpContext& context, PacketSender& sender);

		std::vector<rtcp::ReportBlock> CreateReportBlocks(const FeedbackState& feedback_state);
		
	private:
		bool audio_;
		webrtc::Clock* clock_;
		uint32_t local_ssrc_;
		uint32_t remote_ssrc_;
		int clock_rate_;
		size_t max_packet_size_;
		RtpRtcpModuleObserver* rtp_rtcp_module_observer_;
		std::function<void(webrtc::TimeDelta)> schedule_next_rtcp_send_;
		webrtc::RtcpMode mode_ = webrtc::RtcpMode::kOff;
		webrtc::TimeDelta report_interval_ms_;
		std::set<ReportFlag> report_flags_;
		bool sending_ = false;
		webrtc::Random random_;

		typedef void (RTCPSender::* BuilderFunc)(const RtcpContext& context,
			PacketSender& sender);
		std::map<uint32_t, BuilderFunc> builders_;

		// 最后一个rtp的时间戳
		uint32_t last_rtp_timestamp_ = 0;
		// 最后一个音频或者视频的采集时间
		absl::optional<webrtc::Timestamp> last_frame_capture_time_;
		absl::optional<webrtc::Timestamp> next_time_to_send_rtcp_;

 		std::vector<uint32_t> registered_ssrcs_;
		webrtc::Timestamp last_received_rb_ = webrtc::Timestamp::PlusInfinity();

		ReceiveStatisticsProvider* receive_statistics_ = nullptr;

		RtcpPacketTypeCounter packet_type_counter_;
		RtcpNackStats nack_stats_;
	};


} // namespace xrtc

#endif // MODULES_RTP_RTCP_RTCP_SENDER_H_
