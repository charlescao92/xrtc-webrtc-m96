#include <rtc_base/logging.h>

#include "modules/rtp_rtcp/rtcp_sender.h"
#include "modules/rtp_rtcp/rtcp_packet/sender_report.h"
#include "modules/rtp_rtcp/rtcp_packet/receiver_report.h"
#include "modules/rtp_rtcp/rtp_utils.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/include/receive_statistics_impl.h"
#include "modules/rtp_rtcp/rtcp_packet/nack.h"

namespace xrtc {

namespace {
	const int kDefaultAudioRtcpIntervalMs = 5000;
	const int kDefaultVideoRtcpIntervalMs = 1000;
	constexpr webrtc::TimeDelta RTCP_BEFORE_KEY_FRAME = webrtc::TimeDelta::Millis(100);

	webrtc::TimeDelta GetReportInterval(int report_interval_ms, int default_value) {
		if (report_interval_ms > 0) {
				return webrtc::TimeDelta::Millis(report_interval_ms);
		}
		return webrtc::TimeDelta::Millis(default_value);
	}
}

class RTCPSender::PacketInformation {};
	
class RTCPSender::RtcpContext {
 public:
  RtcpContext(const FeedbackState& feedback_state,
              int32_t nack_size,
              const uint16_t* nack_list,
              webrtc::Timestamp now)
      : feedback_state(feedback_state),
        nack_size(nack_size),
        nack_list(nack_list),
        now(now) {}

  const FeedbackState& feedback_state;
  const int32_t nack_size;
  const uint16_t* nack_list;
  const webrtc::Timestamp now;
};

class RTCPSender::PacketSender {
public:
	PacketSender(rtcp::RtcpPacket::PacketReadyCallback callback,
		size_t max_packet_size) :
		callback_(callback), max_packet_size_(max_packet_size) {}

	~PacketSender() {}

	void Append(rtcp::RtcpPacket& packet) {
		packet.Create(buffer_, &index_, max_packet_size_, callback_);
	}

	void Send() {
		if (index_ > 0) {
			callback_(rtc::ArrayView<const uint8_t>(buffer_, index_));
			index_ = 0;
		}
	}

private:
	rtcp::RtcpPacket::PacketReadyCallback callback_;
	size_t max_packet_size_;
	uint8_t buffer_[IP_PACKET_SIZE];
	size_t index_ = 0;
};

RTCPSender::RTCPSender(const RtpRtcpInterface::Configuration& config,
	std::function<void(webrtc::TimeDelta)> schedule_next_rtcp_send) :
	audio_(config.audio),
	clock_(config.clock),
	local_ssrc_(config.local_ssrc),
	remote_ssrc_(config.remote_ssrc),
	clock_rate_(config.clock_rate),
	max_packet_size_(IP_PACKET_SIZE - 28), // 28 = IPv4 + UDP头
	rtp_rtcp_module_observer_(config.rtp_rtcp_module_observer),
	schedule_next_rtcp_send_(schedule_next_rtcp_send),
	report_interval_ms_(GetReportInterval(config.rtcp_report_interval_ms,
			config.audio ? kDefaultAudioRtcpIntervalMs : kDefaultVideoRtcpIntervalMs)),
	random_(config.clock->TimeInMilliseconds()),
	receive_statistics_(config.receive_statistics)
{
	builders_[kRtcpSr] = &RTCPSender::BuildSR;
	builders_[kRtcpRr] = &RTCPSender::BuildRR;
	builders_[kRtcpNack] = &RTCPSender::BuildNACK;
	registered_ssrcs_.push_back(config.local_ssrc);
}

RTCPSender::~RTCPSender() {}

void RTCPSender::SetLastRtpTimestamp(uint32_t rtp_timestamp, 
		absl::optional<webrtc::Timestamp> last_frame_capture_time)
{
	last_rtp_timestamp_ = rtp_timestamp;
	if (last_frame_capture_time.has_value()) {
		last_frame_capture_time_ = last_frame_capture_time;
	}
	else {
		last_frame_capture_time_ = clock_->CurrentTime();
	}
}

bool RTCPSender::TimeToSendRTCPPacket(bool send_before_keyframe) {
	webrtc::Timestamp now = clock_->CurrentTime();

	if (!audio_ && send_before_keyframe) {
		now += RTCP_BEFORE_KEY_FRAME;
	}

	return now >= *next_time_to_send_rtcp_;
}

int RTCPSender::SendRTCP(const FeedbackState& feedback_state, 
	RTCPPacketType packet_type, 
	size_t nack_size, 
	const uint16_t* nack_list)
{
	absl::optional<PacketSender> sender;
	auto callback = [&](rtc::ArrayView<const uint8_t> packet) {
		// 可以获取打包后的复合包。。
		if (rtp_rtcp_module_observer_) {
			rtp_rtcp_module_observer_->OnLocalRtcpPacket(
				audio_ ? webrtc::MediaType::AUDIO : webrtc::MediaType::VIDEO,
				packet.data(), packet.size());
		}
	};
	sender.emplace(callback, max_packet_size_);

	int ret = ComputeCompoundRTCPPacket(feedback_state, packet_type, nack_size, nack_list, *sender);
		
	// 触发回调
	sender->Send();

	return ret;
}

void RTCPSender::SetRTCPStatus(webrtc::RtcpMode mode) {
	if (mode == webrtc::RtcpMode::kOff) {
		// 关闭RTCP功能是禁止的
		return;
	}
	else if (mode_ == webrtc::RtcpMode::kOff) {	 // 该判断会就进来一次
		SetNextRtcpSendEvaluationDuration(report_interval_ms_ / 2);
	}

	mode_ = mode;
}

void RTCPSender::SetNextRtcpSendEvaluationDuration(webrtc::TimeDelta duration) {
	next_time_to_send_rtcp_ = clock_->CurrentTime() + duration;
	if (schedule_next_rtcp_send_) {
		schedule_next_rtcp_send_(duration);
	}
}

int RTCPSender::ComputeCompoundRTCPPacket(const FeedbackState& feedback_state, 
	RTCPPacketType packet_type, 
	size_t nack_size,
	const uint16_t* nack_list,
	PacketSender& sender)
{
	SetFlag(packet_type);

	// 当没有任何RTP包发送时，需要阻止SR的发送
	bool can_calculate_rtp_timestamp = last_frame_capture_time_.has_value();
	if (!can_calculate_rtp_timestamp) {
		// 此时不能发送SR包
		bool send_sr_flag = ConsumeFlag(kRtcpSr);
		bool send_report_flag = ConsumeFlag(kRtcpReport);
		bool sender_report = send_sr_flag || send_report_flag;
		// 如果当前仅仅只需要发送SR包，我们直接return
		if (sender_report && AllVolatileFlagsConsumed()) {
			return 0;
		}

		// 如果还需要发送其它的RTCP包
		if (sending_ && mode_ == webrtc::RtcpMode::kCompound) {
			// 复合包模式下，发送任何RTCP包都必须携带SR包，
			// 此时又没有发送任何RTP包，不能发送当前的RTCP包
			return -1;
		}
	}

	RtcpContext context(feedback_state, nack_size, nack_list, clock_->CurrentTime());

	PrepareReport(feedback_state);

	// 遍历flag，根据rtcp类型，构造对应的rtcp包
	auto it = report_flags_.begin();
	while (it != report_flags_.end()) {
		uint32_t rtcp_packet_type = it->type;
		if (it->is_volatile) {
			report_flags_.erase(it++);
		}
		else {
			it++;
		}

		// 通过rtcp类型，找到对应的处理函数
		auto builder_it = builders_.find(rtcp_packet_type);
		if (builder_it == builders_.end()) {
			RTC_LOG(LS_WARNING) << "could not find builder for rtcp_packet_type:"
				<< rtcp_packet_type;
		}
		else {
			BuilderFunc func = builder_it->second;
			(this->*func)(context, sender);
		}
	}

	return 0;
}

void RTCPSender::SetFlag(uint32_t type) {
	report_flags_.insert(ReportFlag(type, true));
}

bool RTCPSender::ConsumeFlag(uint32_t type, bool forced) {
	auto it = report_flags_.find(ReportFlag(type, false));
	if (it == report_flags_.end()) {
		return false;
	}

	if (it->is_volatile || forced) {
		report_flags_.erase(it);
	}

	return true;
}

bool RTCPSender::AllVolatileFlagsConsumed() {
	for (auto flag : report_flags_) {
		if (flag.is_volatile) {
				return false;
		}
	}

	return true;
}

void RTCPSender::PrepareReport(const FeedbackState& feedback_state) {
	bool generate_report = true;

	ConsumeFlag(kRtcpReport, false);

	SetFlag(sending_ ? kRtcpSr : kRtcpRr);

	if (generate_report) {
		// 设置下一次发送报告的时间
		int minimal_interval_ms = report_interval_ms_.ms();
		// 下一次发送报告的时间 * [0.5, 1.5]之间随机一个数作为倍数
		webrtc::TimeDelta time_to_next = webrtc::TimeDelta::Millis(
			random_.Rand(minimal_interval_ms * 1 / 2, minimal_interval_ms * 3 / 2)
		);
		SetNextRtcpSendEvaluationDuration(time_to_next);
	}
}

void RTCPSender::BuildSR(const RtcpContext& context, PacketSender& sender) {
	// 计算当前时间的rtp_timestamp
	// last_rtp_timestamp_ -》 last_frame_capture_time_
	//            ?       -》  context.now
	uint32_t rtp_timestamp = last_rtp_timestamp_ +
			((context.now.us() + 500) / 1000 - last_frame_capture_time_->ms()) *
			(clock_rate_ / 1000);

	rtcp::SenderReport sr;
	sr.SetSenderSsrc(local_ssrc_);
	sr.SetNtpTime(clock_->ConvertTimestampToNtpTime(context.now));
	sr.SetRtpTimestamp(rtp_timestamp);
	sr.SetSendPacketCount(context.feedback_state.packets_sent);
	sr.SetSendPacketOctet(context.feedback_state.media_bytes_sent);
		
	sender.Append(sr);
}

void RTCPSender::BuildRR(const RtcpContext& context, PacketSender& sender) {
	rtcp::ReceiverReport report;
  	report.SetSenderSsrc(local_ssrc_);
  	report.SetReportBlocks(CreateReportBlocks(context.feedback_state));

  	sender.Append(report);
}

void RTCPSender::BuildNACK(const RtcpContext& ctx, PacketSender& sender) {
	rtcp::Nack nack;
	nack.SetSenderSsrc(local_ssrc_);
	nack.SetMediaSsrc(remote_ssrc_);
	nack.SetPacketIds(ctx.nack_list, ctx.nack_size);

	// Report stats.
	for (int idx = 0; idx < ctx.nack_size; ++idx) {
		nack_stats_.ReportRequest(ctx.nack_list[idx]);
	}
	packet_type_counter_.nack_requests = nack_stats_.requests();
	packet_type_counter_.unique_nack_requests = nack_stats_.unique_requests();

	++packet_type_counter_.nack_packets;
	sender.Append(nack);
}

std::vector<rtcp::ReportBlock> RTCPSender::CreateReportBlocks( const FeedbackState& feedback_state) {
	std::vector<rtcp::ReportBlock> result;
	if (!receive_statistics_)
    	return result;

	// 里面已经设置了ssrc，累积丢包总数，丢包指数，和jitter抖动值了
  	result = receive_statistics_->RtcpReportBlocks(RTCP_MAX_REPORT_BLOCKS);

  	if (!result.empty() && ((feedback_state.last_rr_ntp_secs != 0) ||
                          (feedback_state.last_rr_ntp_frac != 0))) {
    	
    	uint32_t now = compact_ntp(clock_->CurrentNtpTime());

    	uint32_t receive_time = feedback_state.last_rr_ntp_secs & 0x0000FFFF;
    	receive_time <<= 16;
    	receive_time += (feedback_state.last_rr_ntp_frac & 0xffff0000) >> 16;

    	uint32_t delay_since_last_sr = now - receive_time;
    	
    	for (auto& report_block : result) {
      		report_block.SetLastSr(feedback_state.remote_sr);
      		report_block.SetDelayLastSr(delay_since_last_sr);
    	}
  	}

  	return result;
}

} // end namespace xrtc