#include "modules/video_coding/nack_requester.h"

#include <rtc_base/logging.h>

namespace xrtc {

namespace {

const int kMaxNackRetries = 10;
const int kUpdateIntervalMs = 20;
const int64_t kDefaultRttMs = 100;
const uint16_t kMaxPacketAge = 10000;
const int kMaxNackPackets = 1000;
const int kMaxReorderingPackets = 128;
const int kNumReorderingBuckets = 10;

}

void nack_time_cb(EventLoop*, TimerWatcher*, void* data) {
    NackRequester* requester = (NackRequester*)data;
    requester->ProcessNacks();
}

NackRequester::NackRequester(EventLoop* el, webrtc::Clock* clock) :
    el_(el),
    clock_(clock),
    rtt_ms_(kDefaultRttMs),
    reordering_histogram_(kNumReorderingBuckets, kMaxReorderingPackets)
{
    nack_timer_ = el_->create_timer(nack_time_cb, this, true);
    el_->start_timer(nack_timer_, kUpdateIntervalMs * 1000);     
}

NackRequester::~NackRequester() {
    if (nack_timer_) {
        el_->delete_timer(nack_timer_);
        nack_timer_ = nullptr;
    }
}

int NackRequester::OnReceivedPacket(uint16_t seq_num, bool is_keyframe, bool is_retransmitted) {
    // 第一次收到数据包
    if (!initialized_) {
        newest_seq_num_ = seq_num;
        if (is_keyframe) {
            keyframe_list_.insert(seq_num);
        }
        initialized_ = true;
        return 0;
    }

    // 重复的包
    if (seq_num == newest_seq_num_) {
        return 0;
    }

    // 如果当前的seq_num比我们已经收到最新包的seq_num还要旧
    // 有可能是一个乱序的包，也有可能是一个重传的包
    // webrtc::AheadOf(8, 2) == 1
    // webrtc::AheadOf(65510, 2) == 0
    /* 编译测试
    uint16_t a = 8;
    uint16_t b = 2;
    RTC_LOG(LS_WARNING) << "==========webrtc::AheadOf(8, 2) = " << webrtc::AheadOf(a, b);  // 输出1
    a = 65510;
    RTC_LOG(LS_WARNING) << "==========webrtc::AheadOf(65510, 2) = " << webrtc::AheadOf(a, b); // 输出0
    */
    if (webrtc::AheadOf(newest_seq_num_, seq_num)) {
        // 判断seq_num是否已经在等待重传的列表里面
        // 如果存在，需要删除掉，不需要再重传了
        auto nack_list_it = nack_list_.find(seq_num);
        int nacks_sent_for_packet = 0;
        if (nack_list_it != nack_list_.end()) {
            nacks_sent_for_packet = nack_list_it->second.retries;
            nack_list_.erase(nack_list_it);
        }

        if (!is_retransmitted) {
            // 乱序包
            UpdateReorderingStat(seq_num);
        }

        return nacks_sent_for_packet;
    }

    if (is_keyframe) {
        keyframe_list_.insert(seq_num);
    }
    
    // 避免keyframe_list_长度越来越大，也需要清理
    // 如果keyframe_list_中的seq_num已经比较旧了，可以清理掉
    auto it = keyframe_list_.lower_bound(seq_num - kMaxPacketAge);
    if (it != keyframe_list_.begin()) {
        keyframe_list_.erase(keyframe_list_.begin(), it);
    } 

    // seq_num比我们当前收到的最新的seq_num要新
    AddPacketsToNack(newest_seq_num_ + 1, seq_num);
    newest_seq_num_ = seq_num;

    // 需要触发一次nack，获取丢包，然后发送请求
    std::vector<uint16_t> nack_batch = GetNackBatch(kSeqNumOnly);
    if (!nack_batch.empty()) {
        SignalSendNack(nack_batch);
    }

    return 0;
}

void NackRequester::UpdateReorderingStat(uint16_t seq_num) {
    size_t diff = webrtc::ReverseDiff(newest_seq_num_, seq_num);
    reordering_histogram_.Add(diff);
}

size_t NackRequester::WaitNumberOfPackets(float probability) {
    if (reordering_histogram_.NumValues() == 0) {
        return 0;
    }

    return reordering_histogram_.InverseCdf(probability);
}

bool NackRequester::RemovePacketsUntilKeyFrame() {
    while (!keyframe_list_.empty()) {
        auto it = nack_list_.lower_bound(*keyframe_list_.begin());
        if (it != nack_list_.begin()) {
            nack_list_.erase(nack_list_.begin(), it);
            return true;
        }

        // 由于keyframe_list.begin()指向的seq_num比较久，没有清理掉nack_list中的
        // 任何seq_num, 清理掉这个seq_num，继续下一个keyframe_list元素的清理
        keyframe_list_.erase(keyframe_list_.begin());
    }

    return false;
}

void NackRequester::AddPacketsToNack(uint16_t seq_num_start, uint16_t seq_num_end) { 
    // 清理时间比较久的seq_num
    // 实现了比当前最新的seq_num小于kMaxPacketAge=10000的seq_num的清理
    auto it = nack_list_.lower_bound(seq_num_end - kMaxPacketAge);
    nack_list_.erase(nack_list_.begin(), it);

    // 判断添加完新的seq_num之后，nack_list是否超过限制
    // 1. 计算当前新添加seq_num的个数
    int new_nack_num = webrtc::ForwardDiff(seq_num_start, seq_num_end);
    // 2. 判断是否超过限制
    if (nack_list_.size() + new_nack_num > kMaxNackPackets) {
        // 超过了限制，需要清理
        while (RemovePacketsUntilKeyFrame() &&
                nack_list_.size() + new_nack_num > kMaxNackPackets) {}

        // 尽最大努力清理，但是仍然无法满足要求，放弃重传，直接请求关键帧
        if (nack_list_.size() + new_nack_num > kMaxNackPackets) {
            nack_list_.clear();
            // TODO: 直接请求关键帧
            RTC_LOG(LS_WARNING) << "nack_list full, clear nack_list and request keyframe";
            return;
        }
    }

    // 如果循环能够执行，就说明丢包了
    for (uint16_t seq_num = seq_num_start; seq_num != seq_num_end; ++seq_num) {
        NackInfo nack_info(seq_num, seq_num + WaitNumberOfPackets(0.5),
                clock_->TimeInMilliseconds());
        nack_list_[seq_num] = nack_info;
    }
}

std::vector<uint16_t> NackRequester::GetNackBatch(NackFilterOptions options) {
    bool consider_seq_num = (options != kTimeOnly);
    bool consider_timestamp = (options != kSeqNumOnly);
    std::vector<uint16_t> nack_batch;
    int64_t now = clock_->TimeInMilliseconds();

    auto it = nack_list_.begin();
    while (it != nack_list_.end()) {
        bool delay_timeout = (now - it->second.created_time) >= send_nack_delay_ms_;

        // 判断基于丢包触发nack的条件是否满足
        bool can_nack_seq_num_passed = (it->second.send_at_time == -1) &&
            webrtc::AheadOf(newest_seq_num_, it->second.send_at_seq_num);

        // 判断基于定时触发nack的条件是否满足
        // 保证上一次的重传有充分的时间, 两次重传的间隔设置为rtt的时间
        bool can_nack_timestamp_passed = (now - it->second.send_at_time) > rtt_ms_;

        if (delay_timeout && ((consider_seq_num && can_nack_seq_num_passed) ||
            (consider_timestamp && can_nack_timestamp_passed))) 
        {
            // 触发nack的发送
            nack_batch.emplace_back(it->second.seq_num);
            ++it->second.retries;
            it->second.send_at_time = now;

            // 当该包重传的次数已经达到10次，不要再重传了
            if (it->second.retries >= kMaxNackRetries) {
                nack_list_.erase(it++);
                RTC_LOG(LS_WARNING) << "sequence number: " << it->second.seq_num
                    << " removed from nack list due to max retries";
            } else {
                ++it;
            }

            continue;
        } 
        ++it;
    } 
    return nack_batch;
}

void NackRequester::ProcessNacks() {
    auto nack_batch = GetNackBatch(kTimeOnly);
    if (!nack_batch.empty()) {
        SignalSendNack(nack_batch);
    }
}

} // namespace xrtc
