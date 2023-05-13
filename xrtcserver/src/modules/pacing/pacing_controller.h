#ifndef MODULES_PACING_PACING_CONTROLLER_H
#define MODULES_PACING_PACING_CONTROLLER_H

#include <system_wrappers/include/clock.h>
#include <api/units/data_rate.h>

#include "modules/rtp_rtcp/rtp_packet_to_send.h"
#include "modules/pacing/round_robin_packet_queue.h"
#include "modules/pacing/interval_budget.h"

namespace xrtc {

    class PacingController {
    public:
        class PacketSender {
        public:
            virtual ~PacketSender() = default;
            virtual void SendPacket(std::unique_ptr<RtpPacketToSend> packet) = 0;
        };

        PacingController(webrtc::Clock* clock, PacketSender* packet_sender);
        ~PacingController();

        void EnqueuePacket(std::unique_ptr<RtpPacketToSend> packet);
        void ProcessPackets();
        webrtc::Timestamp NextSendTime();
        void SetPacingBitrate(webrtc::DataRate bitrate);
        void SetQueueTimeLimit(webrtc::TimeDelta limit) {
            queue_time_limit_ = limit;
        }

    private:
        void EnqueuePacketInternal(int priority, std::unique_ptr<RtpPacketToSend> packet);
        webrtc::TimeDelta UpdateTimeAndGetElapsed(webrtc::Timestamp now);
        void UpdateBudgetWithElapsedTime(webrtc::TimeDelta elapsed_time);
        std::unique_ptr<RtpPacketToSend> GetPendingPacket();
        void OnPacketSent(webrtc::DataSize packet_size, webrtc::Timestamp target_send_time);
        void UpdateBudgetWithSendData(webrtc::DataSize packet_size);

    private:
        webrtc::Clock* clock_;
        uint64_t packet_counter_ = 0;
        webrtc::Timestamp last_process_time_;
        RoundRobinPacketQueue packet_queue_;
        webrtc::TimeDelta min_packet_limit_;
        IntervalBudget media_budget_;
        webrtc::DataRate pacing_bitrate_;
        PacketSender* packet_sender_;
        // 当我们队列比较大的时候，是否要启用排空的功能
        bool drain_large_queue_ = true;
        // 期望的最大延迟时间
        webrtc::TimeDelta queue_time_limit_;
    };

} // namespace xrtc

#endif // MODULES_PACING_PACING_CONTROLLER_H