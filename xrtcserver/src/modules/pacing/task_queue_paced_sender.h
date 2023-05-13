#ifndef MODULES_PACING_TASK_QUEUE_PACED_SENDER_H_
#define MODULES_PACING_TASK_QUEUE_PACED_SENDER_H_

#include <system_wrappers/include/clock.h>
#include <api/task_queue/task_queue_factory.h>
#include <rtc_base/task_queue.h>

#include "modules/rtp_rtcp/rtp_packet_to_send.h"
#include "modules/pacing/pacing_controller.h"

namespace xrtc {

    class TaskQueuePacedSender {
    public:
        TaskQueuePacedSender(webrtc::Clock* clock,
            PacingController::PacketSender* packet_sender,
            webrtc::TaskQueueFactory* task_queue_factory,
            webrtc::TimeDelta hold_back_window);
        ~TaskQueuePacedSender();

        void EnsureStarted();

        void EnqueuePacket(std::unique_ptr<RtpPacketToSend> packet);
        void MaybeProcessPackets(webrtc::Timestamp scheduled_process_time);

    private:
        webrtc::Clock* clock_;
        rtc::TaskQueue task_queue_;
        PacingController pacing_controller_;
        // 初始值是负的无穷大，表示暂时还没有调度任何任务
        webrtc::Timestamp next_process_time_ = webrtc::Timestamp::MinusInfinity();
        // 最小的调度周期
        webrtc::TimeDelta hold_back_window_;
    };

} // namespace xrtc

#endif // MODULES_PACING_TASK_QUEUE_PACED_SENDER_H_