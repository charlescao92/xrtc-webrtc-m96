#include "modules/rtp_rtcp/rtp_rtcp_impl.h"

#include <rtc_base/thread.h>
#include <rtc_base/task_utils/to_queued_task.h>
#include <rtc_base/time_utils.h>
#include <rtc_base/logging.h>

namespace xrtc {

constexpr webrtc::TimeDelta kRttUpdateInterval = webrtc::TimeDelta::Millis(1000);
const int kDefaultAudioRtcpIntervalMs = 5000;
const int kDefaultVideoRtcpIntervalMs = 1000;

void send_rr_rtcp_time_cb(EventLoop*, TimerWatcher*, void* data) {
    ModuleRtpRtcpImpl* impl = (ModuleRtpRtcpImpl*)data;
    impl->SendRTCPReceiveReport();
}

ModuleRtpRtcpImpl::ModuleRtpRtcpImpl(
    EventLoop* el,
    const RtpRtcpInterface::Configuration& config) :
    el_(el),
    config_(config),
    clock_(config.clock),
    rtcp_sender_(config, [=](webrtc::TimeDelta duration) {
        ScheduleNextRtcpSend(duration);
    }),
    rtcp_receiver_(config)
{
    int report_interval_ms = config.audio ? kDefaultAudioRtcpIntervalMs : kDefaultVideoRtcpIntervalMs;
    send_rr_rtcp_timer_ = el_->create_timer(send_rr_rtcp_time_cb, this, true);
    el_->start_timer(send_rr_rtcp_timer_, report_interval_ms * 1000);  
}

ModuleRtpRtcpImpl::~ModuleRtpRtcpImpl() {
    if (send_rr_rtcp_timer_) {
        el_->delete_timer(send_rr_rtcp_timer_);
        send_rr_rtcp_timer_ = nullptr;
    }
}

void ModuleRtpRtcpImpl::UpdateRtpStats(std::shared_ptr<RtpPacketToSend> packet,
         bool is_rtx, bool is_retransmit)
{
    StreamDataCounters* stream_counter = is_rtx ? &rtx_rtp_stats_ : &rtp_stats_;

    RtpPacketCounter counter(*packet);
    if (is_retransmit) {
        stream_counter->retransmitted.Add(counter);
    }

    stream_counter->transmitted.Add(counter);
}

void ModuleRtpRtcpImpl::SetRTCPStatus(webrtc::RtcpMode mode) {
    rtcp_sender_.SetRTCPStatus(mode);
}

void ModuleRtpRtcpImpl::SetSendingStatus(bool sending) {
    rtcp_sender_.SetSendingStatus(sending);
}

void ModuleRtpRtcpImpl::OnSendingRtpFrame(uint32_t rtp_timestamp, 
        int64_t capture_time_ms,
        bool forced_report)
{
    absl::optional<webrtc::Timestamp> capture_time;
    if (capture_time_ms > 0) {
        capture_time = webrtc::Timestamp::Millis(capture_time_ms);
    }

    rtcp_sender_.SetLastRtpTimestamp(rtp_timestamp, capture_time);

    if (rtcp_sender_.TimeToSendRTCPPacket(forced_report)) {
        rtcp_sender_.SendRTCP(GetFeedbackState(), kRtcpReport);
    }
}

void ModuleRtpRtcpImpl::IncomingRtcpPacket(rtc::ArrayView<const uint8_t> packet) {
    rtcp_receiver_.IncomingRtcpPacket(packet);
}

void ModuleRtpRtcpImpl::ScheduleNextRtcpSend(webrtc::TimeDelta duration) {
    if (duration.IsZero()) {
        MaybeSendRTCP();
    }
    else {
        webrtc::Timestamp execute_time = config_.clock->CurrentTime() + duration;
        ScheduleMaybeSendRtcpAtOrAfterTimestamp(execute_time, duration);
    }
}

void ModuleRtpRtcpImpl::MaybeSendRTCP() {
    if (rtcp_sender_.TimeToSendRTCPPacket()) {
        rtcp_sender_.SendRTCP(GetFeedbackState(), RTCPPacketType::kRtcpReport);
    }
}

static int DelayMillisForDuration(webrtc::TimeDelta duration) {
    return (duration.us() + rtc::kNumMillisecsPerSec - 1) / rtc::kNumMicrosecsPerMillisec;
}

void ModuleRtpRtcpImpl::ScheduleMaybeSendRtcpAtOrAfterTimestamp(
        webrtc::Timestamp execute_time,
        webrtc::TimeDelta duration)
{
    // 使用延迟调用的功能
    rtc::Thread::Current()->PostDelayedTask(webrtc::ToQueuedTask(task_safety_, [=]() {
        webrtc::Timestamp now = config_.clock->CurrentTime();
        if (now >= execute_time) { // 时间到了就直接执行
            MaybeSendRTCP();
            return;
        }

        // 时间不到就继续循环调用自己
        ScheduleMaybeSendRtcpAtOrAfterTimestamp(execute_time, execute_time - now);

    }), DelayMillisForDuration(duration));
}

RTCPSender::FeedbackState ModuleRtpRtcpImpl::GetFeedbackState() {
    RTCPSender::FeedbackState state;
    if (!config_.receiver_only) {
        state.packets_sent = rtp_stats_.transmitted.packets +
                rtx_rtp_stats_.transmitted.packets;
        state.media_bytes_sent = rtp_stats_.transmitted.payload_bytes
                + rtx_rtp_stats_.transmitted.payload_bytes;
    }

    state.receiver = &rtcp_receiver_;

    uint32_t received_ntp_secs = 0;
    uint32_t received_ntp_frac = 0;
    state.remote_sr = 0;
    if (rtcp_receiver_.NTP(&received_ntp_secs, &received_ntp_frac,
                         /*rtcp_arrival_time_secs=*/&state.last_rr_ntp_secs,
                         /*rtcp_arrival_time_frac=*/&state.last_rr_ntp_frac,
                         /*rtcp_timestamp=*/nullptr,
                         /*remote_sender_packet_count=*/nullptr,
                         /*remote_sender_octet_count=*/nullptr,
                         /*remote_sender_reports_count=*/nullptr)) {
        state.remote_sr = ((received_ntp_secs & 0x0000ffff) << 16) +
                      ((received_ntp_frac & 0xffff0000) >> 16);
    }

    return state;
}

void ModuleRtpRtcpImpl::SendRTCPReceiveReport() {
    rtcp_sender_.SendRTCP(GetFeedbackState(), RTCPPacketType::kRtcpRr);
}

void ModuleRtpRtcpImpl::SendNack(const std::vector<uint16_t>& sequence_numbers) {
    rtcp_sender_.SendRTCP(GetFeedbackState(), RTCPPacketType::kRtcpNack, sequence_numbers.size(),
                        sequence_numbers.data());
}
 
} // end namespace xrtc
