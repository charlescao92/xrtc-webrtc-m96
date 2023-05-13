
#ifndef  __MODULES_VIDEO_CODING_NACK_REQUESTER_H_
#define  __MODULES_VIDEO_CODING_NACK_REQUESTER_H_

#include <map>
#include <vector>
#include <set>

#include <system_wrappers/include/clock.h>
#include <rtc_base/numerics/sequence_number_util.h>
#include <rtc_base/third_party/sigslot/sigslot.h>

#include "base/event_loop.h"
#include "modules/video_coding/histogram.h"

namespace xrtc {

class NackRequester {
public:
    NackRequester(EventLoop* el, webrtc::Clock* clock);
    ~NackRequester();
   
    int OnReceivedPacket(uint16_t seq_num, bool is_keyframe, bool is_retransmitted);
    sigslot::signal1<const std::vector<uint16_t>&> SignalSendNack;
    
    void ProcessNacks();
    void UpdateRtt(int64_t rtt_ms) { rtt_ms_ = rtt_ms; }

private:
    enum NackFilterOptions {
        kSeqNumOnly,    // 基于丢包时触发
        kTimeOnly,      // 定时触发
        kSeqNumAndTime, // 同时触发
    };

    struct NackInfo {
        NackInfo() : seq_num(0), send_at_seq_num(0),
            created_time(-1), send_at_time(-1), 
            retries(0) {}
        NackInfo(uint16_t seq_num, uint16_t send_at_seq_num, int64_t created_time) :
            seq_num(seq_num), 
            send_at_seq_num(send_at_seq_num),
            created_time(created_time),
            send_at_time(-1), retries(0) {}
        
        uint16_t seq_num;
        uint16_t send_at_seq_num;
        int64_t created_time;
        int64_t send_at_time;
        int retries;
    };

private:
    std::vector<uint16_t> GetNackBatch(NackFilterOptions options);  
    bool RemovePacketsUntilKeyFrame();
    void UpdateReorderingStat(uint16_t seq_num);
    size_t WaitNumberOfPackets(float probability);
    void AddPacketsToNack(uint16_t seq_num_start, uint16_t seq_num_end);

private:
    EventLoop* el_;
    webrtc::Clock* clock_;
    bool initialized_ = false;
    uint16_t newest_seq_num_ = 0;
    std::map<uint16_t, NackInfo, webrtc::DescendingSeqNumComp<uint16_t>> nack_list_;
    TimerWatcher* nack_timer_ = nullptr;
    
    int64_t rtt_ms_;
    std::set<uint16_t, webrtc::DescendingSeqNumComp<uint16_t>> keyframe_list_;
    int64_t send_nack_delay_ms_ = 0;
    Histogram reordering_histogram_;

};

} // namespace xrtc

#endif  //__MODULES_VIDEO_CODING_NACK_REQUESTER_H_
