#include "xrtc/media/stats_collector.h"

namespace xrtc {

CRtcStatsCollector::CRtcStatsCollector(StatsObserver* observer)
    : observer_(observer) {}

void CRtcStatsCollector::OnStatsDelivered(const rtc::scoped_refptr<const webrtc::RTCStatsReport>& report) {
    if (observer_) {
        observer_->OnStatsInfo(report);
    }
}

} // namespace xrtc