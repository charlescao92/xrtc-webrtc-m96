#include "xrtc/media/xrtc_push_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include <absl/memory/memory.h>
#include <absl/types/optional.h>
#include <api/audio/audio_mixer.h>
#include <api/audio_codecs/audio_decoder_factory.h>
#include <api/audio_codecs/audio_encoder_factory.h>
#include <api/audio_codecs/builtin_audio_decoder_factory.h>
#include <api/audio_codecs/builtin_audio_encoder_factory.h>
#include <api/audio_options.h>
#include <api/create_peerconnection_factory.h>
#include <api/rtp_sender_interface.h>
#include <api/video_codecs/builtin_video_decoder_factory.h>
#include <api/video_codecs/builtin_video_encoder_factory.h>
#include <api/video_codecs/video_decoder_factory.h>
#include <api/video_codecs/video_encoder_factory.h>
#include <modules/audio_device/include/audio_device.h>
#include <modules/audio_processing/include/audio_processing.h>
#include <modules/video_capture/video_capture.h>
#include <modules/video_capture/video_capture_factory.h>
#include <pc/video_track_source.h>
#include <rtc_base/checks.h>
#include <rtc_base/logging.h>
#include <rtc_base/ref_counted_object.h>
#include <rtc_base/strings/json.h>

#include "xrtc/media/default.h"
#include "xrtc/device/vcm_capturer.h"
#include "xrtc/base/xrtc_http.h"
#include "xrtc/base/xrtc_global.h"
#include "xrtc/device/audio_track.h"
#include "xrtc/tools/timer.h"
#include "xrtc/base/xrtc_json.h"


namespace xrtc {

KRTCPushImpl::KRTCPushImpl(const std::string& server_addr, const std::string& uid, const std::string& stream_name) :
    KRTCMediaBase(CONTROL_TYPE::PUSH, server_addr, uid, stream_name)
{
    XRTCGlobal::Instance()->http_manager()->AddObject(this);
}

KRTCPushImpl::~KRTCPushImpl() {
    //Check failed: this->IsInvokeToThreadAllowed(this)
   /* XRTCGlobal::Instance()->api_thread()->Invoke<void>(RTC_FROM_HERE, [=]() {
        RTC_DCHECK(!peer_connection_);
    }); */
}

void KRTCPushImpl::Start() {
    RTC_LOG(LS_INFO) << "KRTCPushImpl Start";

    webrtc::PeerConnectionInterface::RTCConfiguration config;
    config.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;
    config.enable_dtls_srtp = true;

    webrtc::PeerConnectionFactoryInterface* peer_connection_factory =
        XRTCGlobal::Instance()->push_peer_connection_factory();
    peer_connection_ = peer_connection_factory->CreatePeerConnection(
        config, nullptr, nullptr, this);

    webrtc::RtpTransceiverInit rtpTransceiverInit;
    rtpTransceiverInit.direction = webrtc::RtpTransceiverDirection::kSendOnly;
    peer_connection_->AddTransceiver(cricket::MediaType::MEDIA_TYPE_AUDIO, rtpTransceiverInit);
    peer_connection_->AddTransceiver(cricket::MediaType::MEDIA_TYPE_VIDEO, rtpTransceiverInit);

    cricket::AudioOptions options;
    rtc::scoped_refptr<LocalAudioSource> audio_source(LocalAudioSource::Create(&options));

    rtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track(
            peer_connection_factory->CreateAudioTrack(kAudioLabel, audio_source));
    auto add_audio_track_result = peer_connection_->AddTrack(audio_track, { kStreamId });
    if (!add_audio_track_result.ok()) {
        RTC_LOG(LS_ERROR) << "Failed to add audio track to PeerConnection: "
            << add_audio_track_result.error().message();
    }

    rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track(
        peer_connection_factory->CreateVideoTrack(
            kAudioLabel, XRTCGlobal::Instance()->current_video_source()));
    auto add_video_track_result = peer_connection_->AddTrack(video_track, { kStreamId });
    if (!add_video_track_result.ok()) {
        RTC_LOG(LS_ERROR) << "Failed to add video track to PeerConnection: "
            << add_video_track_result.error().message();

    }

    if (!add_audio_track_result.ok() && !add_video_track_result.ok()) {
        if (XRTCGlobal::Instance()->engine_observer()) {
            XRTCGlobal::Instance()->engine_observer()->OnPushFailed(KRTCError::kAddTrackErr);
        }

        return;
    }

    peer_connection_->CreateOffer(
        this, webrtc::PeerConnectionInterface::RTCOfferAnswerOptions());

}

void KRTCPushImpl::Stop()
{
     SendStopRequest();

     if (peer_connection_) {
         peer_connection_ = nullptr;
     }

    if (stats_timer_) {
        stats_timer_->Stop();
        stats_timer_ = nullptr;
    }
}

void KRTCPushImpl::GetRtcStats() {
    XRTCGlobal::Instance()->api_thread()->PostTask(webrtc::ToQueuedTask([=]() {
        RTC_LOG(LS_INFO) << "KRTCPushImpl  GetRtcStats";
        if (!stats_) {
            stats_ = new rtc::RefCountedObject<CRtcStatsCollector>(this);
        }
        if (peer_connection_) {
            peer_connection_->GetStats(stats_.get());
        }
     }));
}

void KRTCPushImpl::OnDataChannel(
    rtc::scoped_refptr<webrtc::DataChannelInterface> channel) {
}

// CreateSessionDescriptionObserver implementation.
void KRTCPushImpl::OnSuccess(webrtc::SessionDescriptionInterface* desc) {
    peer_connection_->SetLocalDescription(
        DummySetSessionDescriptionObserver::Create(), desc);

    std::string sdpOffer;
    desc->ToString(&sdpOffer);
    RTC_LOG(INFO) << "sdp offer:" << sdpOffer;

    // https://www.charlescao92.cn:8081/signaling/push?uid=xxx&streamName=xxx&audio=1&video=1&isDtls=1&sdp="xxx"
    // ¹¹Ôìbody
    std::stringstream body;
    body << "uid=" + uid_
        << "&streamName=" + stream_name_
        << "&audio=1&video=1&isDtls=1"
        << "&sdp=" + HttpManager::UrlEncode(sdpOffer);
    
    RTC_LOG(LS_INFO) << "push url: " << server_addr_ << " request body:" << body.str();

    std::string url = server_addr_ + "/signaling/push";
    HttpRequest request(url, body.str());

    XRTCGlobal::Instance()->http_manager()->Post(request, [=](HttpReply reply) {
      RTC_LOG(LS_INFO) << "signaling push response, url: " << reply.get_url()
            << ", body: " << reply.get_body()
            << ", status: " << reply.get_status_code()
            << ", err_no: " << reply.get_errno()
            << ", err_msg: " << reply.get_err_msg()
            << ", response: " << reply.get_resp();
       XRTCGlobal::Instance()->api_thread()->PostTask(webrtc::ToQueuedTask([=]() {
           HandleHttpPushResponse(reply);
        }));

    }, this);

}

void KRTCPushImpl::OnFailure(webrtc::RTCError error) {
    if (XRTCGlobal::Instance()->engine_observer()) {
        XRTCGlobal::Instance()->engine_observer()->OnPushFailed(KRTCError::kCreateOfferErr);
    }
}

void KRTCPushImpl::OnStatsInfo(const rtc::scoped_refptr<const webrtc::RTCStatsReport>& report) {
    Json::Reader reader;

    for (auto it = report->begin(); it != report->end(); ++it) {
        Json::Value jmessage;
        if (!reader.parse(it->ToJson(), jmessage)) {
            RTC_LOG(WARNING) << "stats report invalid!!!";
            return;
        }

        std::string type = jmessage["type"].asString();
        if (type == "outbound-rtp") {

            uint64_t rtt_ms = jmessage["rttMs"].asUInt64();
            uint64_t packetsLost = jmessage["packetsLost"].asUInt64();
            double fractionLost = jmessage["fractionLost"].asDouble();

            if (XRTCGlobal::Instance()->engine_observer()) {
                XRTCGlobal::Instance()->engine_observer()->OnNetworkInfo(rtt_ms, packetsLost, fractionLost);
            }
        }
    }
}

bool KRTCPushImpl::ParseReply(const HttpReply& reply, std::string& type, std::string& sdp)
{
    JsonValue value;
    if (!value.FromJson(reply.get_resp())) {
        RTC_LOG(LS_WARNING) << "invalid json response";
        return false;
    }

    JsonObject jobj = value.ToObject();
    int err_no = jobj["errNo"].ToInt();
    if (err_no != 0) {
        RTC_LOG(LS_WARNING) << "response errNo is not 0, err_no: " << err_no;
        return false;
    }

    JsonObject data = jobj["data"].ToObject();
    type = data["type"].ToString();
    sdp = data["sdp"].ToString();

    if (sdp.empty()) {
        RTC_LOG(LS_WARNING) << "sdp is empty";
        return false;
    }

    return true;
}

void KRTCPushImpl::HandleHttpPushResponse(const HttpReply &reply) {
    if (reply.get_status_code() != 200 || reply.get_errno() != 0) {
        if (XRTCGlobal::Instance()->engine_observer()) {
            XRTCGlobal::Instance()->engine_observer()->OnPushFailed(KRTCError::kSendOfferErr);
        }
        return;
    }

    std::string type1;
    std::string sdpAnswer;
    if (!ParseReply(reply, type1, sdpAnswer)) {
        RTC_LOG(WARNING) << "Received unknown message";

        if (XRTCGlobal::Instance()->engine_observer()) {
            XRTCGlobal::Instance()->engine_observer()->OnPushFailed(KRTCError::kParseAnswerErr);
        }
        return;
    }

    webrtc::SdpParseError error;
    webrtc::SdpType type = webrtc::SdpType::kAnswer;
    std::unique_ptr<webrtc::SessionDescriptionInterface> session_description =
        webrtc::CreateSessionDescription(type, sdpAnswer, &error);

    peer_connection_->SetRemoteDescription(
        DummySetSessionDescriptionObserver::Create(),
        session_description.release());

    if (XRTCGlobal::Instance()->engine_observer()) {
        XRTCGlobal::Instance()->engine_observer()->OnPushSuccess();
    }

   assert(stats_timer_ == nullptr);
    stats_timer_ = std::make_unique<CTimer>(1 * 1000, true, [this]() {
          GetRtcStats();
    });
    stats_timer_->Start();
}

void KRTCPushImpl::SendStopRequest()
{
    // https://www.charlescao92.cn/signaling/stoppush?uid=xxx&streamName=xxx
    std::stringstream body;
    body << "uid=" + uid_
        << "&streamName=" + stream_name_;

    RTC_LOG(LS_INFO) << "stop push url: " << server_addr_ << ", request body:" << body.str();

    std::string url = server_addr_ + "/signaling/stoppush";
    HttpRequest request(url, body.str());

    XRTCGlobal::Instance()->http_manager()->Post(request, [=](HttpReply reply) {
        RTC_LOG(LS_INFO) << "signaling stoppush response, url: " << reply.get_url()
            << ", body: " << reply.get_body()
            << ", status: " << reply.get_status_code()
            << ", err_no: " << reply.get_errno()
            << ", err_msg: " << reply.get_err_msg()
            << ", response: " << reply.get_resp();

        if (reply.get_status_code() != 200 || reply.get_errno() != 0) {
            RTC_LOG(LS_WARNING) << "signaling stoppush response error";
            return;
        }

        JsonValue value;
        if (!value.FromJson(reply.get_resp())) {
            RTC_LOG(LS_WARNING) << "invalid json response";
            return;
        }

        JsonObject jobj = value.ToObject();
        int err_no = jobj["errNo"].ToInt();
        if (err_no != 0) {
            RTC_LOG(LS_WARNING) << "response errNo is not 0, err_no: " << err_no;
            return;
        }

        }, this);
}

} // namespace xrtc
