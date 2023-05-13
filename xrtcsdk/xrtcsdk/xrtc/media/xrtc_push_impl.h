
#ifndef KRTCSDK_KRTC_MEDIA_KRTC_PUSH_IMPL_H_
#define KRTCSDK_KRTC_MEDIA_KRTC_PUSH_IMPL_H_

#include <deque>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <api/media_stream_interface.h>

#include "xrtc/media/xrtc_media_base.h"
#include "xrtc/media/stats_collector.h"
#include "xrtc/base/xrtc_http.h"

class CTimer;

namespace xrtc {

class KRTCPushImpl : public KRTCMediaBase,
                     public webrtc::PeerConnectionObserver,
                     public webrtc::CreateSessionDescriptionObserver,
                     public StatsObserver
{
public:
    explicit KRTCPushImpl(const std::string& server_addr, const std::string& uid, const std::string& stream_name);
    ~KRTCPushImpl();

    void Start();
    void Stop();
    void GetRtcStats();

private:
    // PeerConnectionObserver implementation.
    void OnSignalingChange(
        webrtc::PeerConnectionInterface::SignalingState new_state) override {}

    void OnDataChannel(
        rtc::scoped_refptr<webrtc::DataChannelInterface> channel) override;

    void OnIceGatheringChange(
        webrtc::PeerConnectionInterface::IceGatheringState new_state) override {}

    void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override {}

    // CreateSessionDescriptionObserver implementation.
    void OnSuccess(webrtc::SessionDescriptionInterface* desc) override;

    void OnFailure(webrtc::RTCError error) override;

    // StatsObserver implementation.
    void OnStatsInfo(const rtc::scoped_refptr<const webrtc::RTCStatsReport>& report);

    bool ParseReply(const HttpReply& reply, std::string& type, std::string& sdp);
    void HandleHttpPushResponse(const HttpReply& reply);

    void SendStopRequest();

private:
    rtc::scoped_refptr<CRtcStatsCollector> stats_;
    std::unique_ptr<CTimer> stats_timer_;

};

} // namespace xrtc

#endif  // KRTCSDK_KRTC_MEDIA_KRTC_PUSH_IMPL_H_
