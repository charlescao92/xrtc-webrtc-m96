#include "modules/pacing/task_queue_paced_sender.h"

namespace xrtc {

    TaskQueuePacedSender::TaskQueuePacedSender(webrtc::Clock* clock,
        PacingController::PacketSender* packet_sender,
        webrtc::TaskQueueFactory* task_queue_factory,
        webrtc::TimeDelta hold_back_window) :
        clock_(clock),
        task_queue_(task_queue_factory->CreateTaskQueue("TaskQueuePacedSender",
            webrtc::TaskQueueFactory::Priority::NORMAL)),
        pacing_controller_(clock_, packet_sender),
        hold_back_window_(hold_back_window)
    {
        // 为了测试pacer模块的功能
        pacing_controller_.SetPacingBitrate(webrtc::DataRate::KilobitsPerSec(800));
    }

    TaskQueuePacedSender::~TaskQueuePacedSender() {
    }

    void TaskQueuePacedSender::EnsureStarted() {
        task_queue_.PostTask([this]() {
            MaybeProcessPackets(webrtc::Timestamp::MinusInfinity());
        });
    }

    void TaskQueuePacedSender::EnqueuePacket(std::unique_ptr<RtpPacketToSend> packet) {
        task_queue_.PostTask([this, packet_ = std::move(packet)]() mutable {
            pacing_controller_.EnqueuePacket(std::move(packet_));
        });
    }

    void TaskQueuePacedSender::MaybeProcessPackets(
        webrtc::Timestamp scheduled_process_time)
    {
        webrtc::Timestamp next_process_time = pacing_controller_.NextSendTime();
        bool is_sheculded_call = (scheduled_process_time == next_process_time_);
        if (is_sheculded_call) {
            // 当前的任务将被执行，需要重新设定下一次任务执行的时间
            next_process_time_ = webrtc::Timestamp::MinusInfinity();
            // 执行数据包发送逻辑
            pacing_controller_.ProcessPackets();
            next_process_time = pacing_controller_.NextSendTime();
        }

        webrtc::Timestamp now = clock_->CurrentTime();
        // 需要过多长时间之后，再次进行这个调度
        absl::optional<webrtc::TimeDelta> time_to_next_send;
        if (next_process_time_.IsMinusInfinity()) {
            // 当前还没有设定下一次的调度任务，需要创建一个
            // 为了避免粒度非常小，所以加个限制hold_back_window_
            time_to_next_send = std::max(next_process_time - now, hold_back_window_);
        }

        if (time_to_next_send) {
            next_process_time_ = next_process_time;

            task_queue_.PostDelayedTask([this, next_process_time]() {
                MaybeProcessPackets(next_process_time);
            }, time_to_next_send->ms<uint32_t>());
        }

    }

} // end namespace xrtc