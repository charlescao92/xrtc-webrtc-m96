#ifndef MODULES_RTP_RTCP_RTP_RTCP_IMPL_H_
#define MODULES_RTP_RTCP_RTP_RTCP_IMPL_H_

#include <rtc_base/task_utils/pending_task_safety_flag.h>
#include <api/task_queue/task_queue_base.h>
#include <rtc_base/synchronization/mutex.h>

#include "base/event_loop.h"
#include "modules/rtp_rtcp/rtp_rtcp_interface.h"
#include "modules/rtp_rtcp/rtp_packet_to_send.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/rtcp_sender.h"
#include "modules/rtp_rtcp/rtcp_receiver.h"

namespace xrtc {

    class ModuleRtpRtcpImpl {
    public:
        ModuleRtpRtcpImpl(EventLoop* el, const RtpRtcpInterface::Configuration& config);
        ~ModuleRtpRtcpImpl();

        void UpdateRtpStats(std::shared_ptr<RtpPacketToSend> packet,
            bool is_rtx, bool is_retransmit);
        void SetRTCPStatus(webrtc::RtcpMode mode);
        void SetSendingStatus(bool sending);
        void OnSendingRtpFrame(uint32_t rtp_timestamp,
            int64_t capture_time_ms,
            bool forced_report);
        void IncomingRtcpPacket(const uint8_t* packet, size_t length) {
            IncomingRtcpPacket(rtc::MakeArrayView(packet, length));
        }
        void IncomingRtcpPacket(rtc::ArrayView<const uint8_t> packet);

        // 周期发送RTCP RR包
        void SendRTCPReceiveReport();

        void SendNack(const std::vector<uint16_t>& sequence_numbers);

    private:
        void ScheduleNextRtcpSend(webrtc::TimeDelta duration);
        void MaybeSendRTCP();
        void ScheduleMaybeSendRtcpAtOrAfterTimestamp(
            webrtc::Timestamp execute_time,
            webrtc::TimeDelta duration);
        RTCPSender::FeedbackState GetFeedbackState();

    private:
        EventLoop* el_;
        TimerWatcher* send_rr_rtcp_timer_;
        RtpRtcpInterface::Configuration config_;
        StreamDataCounters rtp_stats_;
        StreamDataCounters rtx_rtp_stats_;

        RTCPSender rtcp_sender_;
        RTCPReceiver rtcp_receiver_;

        webrtc::ScopedTaskSafety task_safety_;

        webrtc::Clock* const clock_;
    };

} // end namespace xrtc

#endif // MODULES_RTP_RTCP_RTP_RTCP_IMPL_H_