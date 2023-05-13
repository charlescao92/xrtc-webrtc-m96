#include <rtc_base/logging.h>

#include "modules/pacing/pacing_controller.h"

namespace xrtc {
namespace {

    // 固定周期 5毫秒
    const webrtc::TimeDelta kDefaultMinPacketLimit = webrtc::TimeDelta::Millis(5);
    const webrtc::TimeDelta kMaxElapsedTime = webrtc::TimeDelta::Seconds(2);
    const webrtc::TimeDelta kMaxProcessingInterval = webrtc::TimeDelta::Millis(30);
    const webrtc::TimeDelta kMaxExpectedQueueLength = webrtc::TimeDelta::Millis(2000);

    // 值越小，优先级越高
    const int kFirstPriority = 0;

    int GetPriorityForType(RtpPacketMediaType type) {
        switch (type) {
        case RtpPacketMediaType::kAudio:
            return kFirstPriority + 1;
        case RtpPacketMediaType::kRetransmission:
            return kFirstPriority + 2;
        case RtpPacketMediaType::kVideo:
        case RtpPacketMediaType::kForwardErrorCorrection:
            return kFirstPriority + 3;
        case RtpPacketMediaType::kPadding:
            return kFirstPriority + 4;
        }
        return kFirstPriority;
    }

}

    PacingController::PacingController(webrtc::Clock* clock, PacingController::PacketSender* packet_sender) :
        clock_(clock),
        packet_sender_(packet_sender),
        last_process_time_(clock_->CurrentTime()),
        packet_queue_(last_process_time_),
        min_packet_limit_(kDefaultMinPacketLimit),
        media_budget_(0),
        pacing_bitrate_(webrtc::DataRate::Zero()),
        queue_time_limit_(kMaxExpectedQueueLength)
    {
    }

	PacingController::~PacingController() {

	}

    void PacingController::EnqueuePacket(std::unique_ptr<RtpPacketToSend> packet) {
        // 1. 获得RTP packet的优先级
        int priority = GetPriorityForType(*packet->packet_type());
        // 2. 插入packet
        EnqueuePacketInternal(priority, std::move(packet));
    }

    void PacingController::ProcessPackets() {
        webrtc::Timestamp now = clock_->CurrentTime();
        webrtc::Timestamp target_send_time = now;
        // 计算流逝的时间（当前时间距离上一次发送过去了多长时间）
        webrtc::TimeDelta elapsed_time = UpdateTimeAndGetElapsed(now);
        if (elapsed_time > webrtc::TimeDelta::Zero()) {
            webrtc::DataRate target_rate = pacing_bitrate_;
            packet_queue_.UpdateQueueTime(now);
            // 队列当中正在排队的总字节数
            webrtc::DataSize queue_data_size = packet_queue_.Size();
            if (queue_data_size > webrtc::DataSize::Zero()) {
                if (drain_large_queue_) {
                    // 当前队列的平均排队时间
                    webrtc::TimeDelta avg_queue_time = packet_queue_.AverageQueueTime();
                    webrtc::TimeDelta avg_queue_left = std::max(
                        webrtc::TimeDelta::Millis(1),
                        queue_time_limit_ - avg_queue_time);
                    webrtc::DataRate min_rate_need = queue_data_size / avg_queue_left;
                    if (min_rate_need > target_rate) {
                        target_rate = min_rate_need;
                        RTC_LOG(LS_INFO) << "large queue, pacing_rate: " << pacing_bitrate_.kbps()
                            << ", min_rate_need: " << min_rate_need.kbps()
                            << ", queue_data_size: " << queue_data_size.bytes()
                            << ", avg_queue_time: " << avg_queue_time.ms()
                            << ", avg_queue_left: " << avg_queue_left.ms();
                    }
                }
            }

            // 更新预算
            media_budget_.SetTargetBitrateKbps(target_rate.kbps());
            UpdateBudgetWithElapsedTime(elapsed_time);
        }

        while (true) {
            // 从队列当中获取rtp数据包进行发送
            std::unique_ptr<RtpPacketToSend> rtp_packet =
                GetPendingPacket();

            if (!rtp_packet) {
                // 队列为空或者预算耗尽了，停止发送循环
                break;
            }

            webrtc::DataSize packet_size = webrtc::DataSize::Bytes(
                rtp_packet->payload_size() + rtp_packet->padding_size());

            // 发送rtp_packet到网络
            packet_sender_->SendPacket(std::move(rtp_packet));

            // 更新预算
            OnPacketSent(packet_size, target_send_time);

        }
    }

    webrtc::Timestamp PacingController::NextSendTime() {
        return last_process_time_ + min_packet_limit_;
    }

    void PacingController::SetPacingBitrate(webrtc::DataRate bitrate) {
        pacing_bitrate_ = bitrate;
        RTC_LOG(LS_INFO) << "pacing bitrate update, pacing_bitrate_kbps: "
            << pacing_bitrate_.kbps();
    }

    void PacingController::EnqueuePacketInternal(int priority, 
        std::unique_ptr<RtpPacketToSend> packet)
    {
        webrtc::Timestamp now = clock_->CurrentTime();
        packet_queue_.Push(priority, now, packet_counter_++, std::move(packet));
    }

    webrtc::TimeDelta PacingController::UpdateTimeAndGetElapsed(webrtc::Timestamp now) {
        if (now < last_process_time_) {
            return webrtc::TimeDelta::Zero();
        }

        webrtc::TimeDelta elapsed_time = now - last_process_time_;
        last_process_time_ = now;
        if (elapsed_time > kMaxElapsedTime) {
            elapsed_time = kMaxElapsedTime;
            RTC_LOG(LS_WARNING) << "elapsed time " << elapsed_time.ms()
                << " is longer than expected, limitting to "
                << kMaxElapsedTime.ms();
        }
        return elapsed_time;
    }

    void PacingController::UpdateBudgetWithElapsedTime(
        webrtc::TimeDelta elapsed_time) 
    {
        webrtc::TimeDelta delta = std::min(elapsed_time, kMaxProcessingInterval);
        media_budget_.IncreaseBudget(delta.ms()); 
    }

    std::unique_ptr<RtpPacketToSend> PacingController::GetPendingPacket() {
        // 如果队列为空
        if (packet_queue_.Empty()) {
            return nullptr;
        }

        // 如果本轮预算已经耗尽
        if (media_budget_.BytesRemaining() <= 0) {
            return nullptr;
        }

        return packet_queue_.Pop();
    }

    void PacingController::OnPacketSent(webrtc::DataSize packet_size,
        webrtc::Timestamp send_time)
    {
        UpdateBudgetWithSendData(packet_size);
        last_process_time_ = send_time;
    }

    void PacingController::UpdateBudgetWithSendData(webrtc::DataSize packet_size) {
        media_budget_.UseBudget(packet_size.bytes());
    }

}
