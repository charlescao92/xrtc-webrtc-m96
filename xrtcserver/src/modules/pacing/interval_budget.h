#ifndef MODULES_PACING_INTERVAL_BUDGET_H_
#define MODULES_PACING_INTERVAL_BUDGET_H_

#include <stdint.h>
#include <stdlib.h>

namespace xrtc {

    class IntervalBudget {
    public:
        IntervalBudget(int initial_target_bitrate_kbps,
            bool can_build_up_underuse = false);
        ~IntervalBudget();

        void SetTargetBitrateKbps(int target_bitrate_kbps);
        void IncreaseBudget(int64_t elapsed_time);
        void UseBudget(size_t bytes);
        size_t BytesRemaining();

    private:
        int target_bitrate_kbps_ = 0;   
        int64_t max_bytes_in_budget_ = 0;
        int64_t bytes_remaining_ = 0;
        bool can_build_up_underuse_ = false;   // 是否允许使用上一轮没有用完的预算值
    };

} // namespace xrtc

#endif // MODULES_PACING_INTERVAL_BUDGET_H_