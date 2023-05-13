#ifndef KRTCSDK_KRTC_MEDIA_STATS_COLLECTOR_H_
#define KRTCSDK_KRTC_MEDIA_STATS_COLLECTOR_H_

#include <pc/rtc_stats_collector.h>
#include <api/stats/rtc_stats_report.h>

#include "xrtc/xrtc.h"

namespace xrtc {

class StatsObserver {
public:
    virtual void OnStatsInfo(const rtc::scoped_refptr<const webrtc::RTCStatsReport>& report) = 0;
};

class CRtcStatsCollector : public webrtc::RTCStatsCollectorCallback {
public:
    CRtcStatsCollector(StatsObserver* observer = nullptr);

private:
    virtual void OnStatsDelivered(
        const rtc::scoped_refptr<const webrtc::RTCStatsReport>& report) override;

private:
    StatsObserver* observer_ = nullptr;
};

} // namespace xrtc

#endif // KRTCSDK_KRTC_MEDIA_STATS_COLLECTOR_H_