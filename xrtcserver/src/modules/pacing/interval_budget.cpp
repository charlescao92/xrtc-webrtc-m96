#include "modules/pacing/interval_budget.h"

#include <algorithm>

namespace xrtc {

    namespace {

        int kWindowMs = 500;

    } // namespace

    IntervalBudget::IntervalBudget(int initial_target_bitrate_kbps,
        bool can_build_up_underuse) :
        target_bitrate_kbps_(initial_target_bitrate_kbps),
        can_build_up_underuse_(can_build_up_underuse)
    {
        SetTargetBitrateKbps(target_bitrate_kbps_);
    }

    IntervalBudget::~IntervalBudget() {
    }

    void IntervalBudget::SetTargetBitrateKbps(int target_bitrate_kbps) {
        target_bitrate_kbps_ = target_bitrate_kbps;
        max_bytes_in_budget_ = (target_bitrate_kbps * kWindowMs) / 8;
        // [-max_bytes_in_budget, max_bytes_in_bydget]
        bytes_remaining_ = std::min(
            std::max(-max_bytes_in_budget_, bytes_remaining_),
            max_bytes_in_budget_);
    }

    void IntervalBudget::IncreaseBudget(int64_t elapsed_time) {
        int64_t bytes = (elapsed_time * target_bitrate_kbps_) / 8;
        if (bytes_remaining_ < 0 || can_build_up_underuse_) {
            // 上一轮预算使用超标，需要在本轮补偿
            // 如果can_build_up_underuse_为TRUE，上一轮的预算可以在后续继续使用
            bytes_remaining_ = std::min(bytes_remaining_ + bytes, max_bytes_in_budget_);
        }
        else {
            // 上一轮预算还有富余，作废
            bytes_remaining_ = std::min(bytes, max_bytes_in_budget_);
        }
    }

    void IntervalBudget::UseBudget(size_t bytes) {
        bytes_remaining_ = std::max(bytes_remaining_ - static_cast<int64_t>(bytes),
            -max_bytes_in_budget_);
    }

    size_t IntervalBudget::BytesRemaining() {
        return std::max<int64_t>(0, bytes_remaining_);
    }


} // namespace xrtc