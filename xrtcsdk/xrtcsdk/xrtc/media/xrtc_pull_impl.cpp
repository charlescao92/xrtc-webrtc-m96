#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>
#include <thread>

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
#include <pc/rtc_stats_collector.h>
#include <rtc_base/strings/json.h>

#include "xrtc/media/default.h"
#include "xrtc/media/xrtc_pull_impl.h"
#include "xrtc/base/xrtc_global.h"
#include "xrtc/base/xrtc_json.h"

namespace xrtc {

void CRtcStatsCollector1::OnStatsDelivered(
    const rtc::scoped_refptr<const webrtc::RTCStatsReport>& report) {
    Json::Reader reader;

    for (auto it = report->begin(); it != report->end(); ++it) {
        // "type" : "inbound-rtp"
        Json::Value jmessage;
        if (!reader.parse(it->ToJson(), jmessage)) {
            RTC_LOG(WARNING) << "stats report invalid!!!";
            return;
        }

        std::string type = jmessage["type"].asString();
        if (type == "inbound-rtp") {

            RTC_LOG(INFO) << "Stats report : " << it->ToJson();
        }
    }
}

KRTCPullImpl::KRTCPullImpl(
    const std::string& server_addr,
    const std::string& uid, 
    const std::string& stream_name,
    const int& hwnd) :
    KRTCMediaBase(CONTROL_TYPE::PULL, server_addr, uid, stream_name, hwnd)
{
    XRTCGlobal::Instance()->http_manager()->AddObject(this);
}

KRTCPullImpl::~KRTCPullImpl() {
    RTC_DCHECK(!peer_connection_);
}

void KRTCPullImpl::Start() {
    RTC_LOG(LS_INFO) << "KRTCPullImpl Start";

    peer_connection_factory_ = webrtc::CreatePeerConnectionFactory(
        XRTCGlobal::Instance()->network_thread() /* network_thread */,
        XRTCGlobal::Instance()->worker_thread() /* worker_thread */,
        XRTCGlobal::Instance()->api_thread() /* signaling_thread */,
        nullptr /* default_adm */,
        webrtc::CreateBuiltinAudioEncoderFactory(),
        webrtc::CreateBuiltinAudioDecoderFactory(),
        webrtc::CreateBuiltinVideoEncoderFactory(),
        webrtc::CreateBuiltinVideoDecoderFactory(), nullptr /* audio_mixer */,
        nullptr /* audio_processing */);

    webrtc::PeerConnectionInterface::RTCConfiguration config;
    config.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;
    config.enable_dtls_srtp = true;

    peer_connection_ = peer_connection_factory_->CreatePeerConnection(
        config, nullptr, nullptr, this);

    webrtc::RtpTransceiverInit rtpTransceiverInit;
    rtpTransceiverInit.direction = webrtc::RtpTransceiverDirection::kRecvOnly;
    peer_connection_->AddTransceiver(cricket::MediaType::MEDIA_TYPE_AUDIO, rtpTransceiverInit);
    peer_connection_->AddTransceiver(cricket::MediaType::MEDIA_TYPE_VIDEO, rtpTransceiverInit);

    peer_connection_->CreateOffer(this, webrtc::PeerConnectionInterface::RTCOfferAnswerOptions());
}

void KRTCPullImpl::Stop() {
    RTC_LOG(LS_INFO) << "KRTCPullImpl Stop";

    SendStopRequest();

    peer_connection_ = nullptr;
    peer_connection_factory_ = nullptr;
    remote_renderer_ = nullptr;
}

void KRTCPullImpl::GetRtcStats() {
    rtc::scoped_refptr<CRtcStatsCollector1> stats(
        new rtc::RefCountedObject<CRtcStatsCollector1>());
    peer_connection_->GetStats(stats);
}

// PeerConnectionObserver implementation.
void KRTCPullImpl::OnSignalingChange(
    webrtc::PeerConnectionInterface::SignalingState new_state) {
}

void KRTCPullImpl::OnAddTrack(
    rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver,
    const std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>&streams)
{
    auto* track = reinterpret_cast<webrtc::MediaStreamTrackInterface*>(
        receiver->track().release());

    if (track->kind() == webrtc::MediaStreamTrackInterface::kVideoKind) {
        auto* video_track = static_cast<webrtc::VideoTrackInterface*>(track);

        remote_renderer_ = VideoRenderer::Create(CONTROL_TYPE::PULL, hwnd_, 1, 1);
        video_track->AddOrUpdateSink(remote_renderer_.get(), rtc::VideoSinkWants());
    }

    track->Release();
}

void KRTCPullImpl::OnRemoveTrack(
    rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver) {
}

void KRTCPullImpl::OnDataChannel(
    rtc::scoped_refptr<webrtc::DataChannelInterface> channel) {
}

void KRTCPullImpl::OnIceGatheringChange(
    webrtc::PeerConnectionInterface::IceGatheringState new_state) {
}

// CreateSessionDescriptionObserver implementation.
void KRTCPullImpl::OnSuccess(webrtc::SessionDescriptionInterface* desc) {

    peer_connection_->SetLocalDescription(
        DummySetSessionDescriptionObserver::Create(), desc);

    std::string sdpOffer;
    desc->ToString(&sdpOffer);
    RTC_LOG(INFO) << "sdp Offer:" << sdpOffer;

    // https://www.charlescao92.cn:8081/signaling/pull?uid=xxx&streamName=xxx&audio=1&video=1&isDtls=1&sdp=xxx
    // ¹¹Ôìbody
    std::stringstream body;
    body << "uid=" + uid_
        << "&streamName=" + stream_name_
        << "&audio=1&video=1&isDtls=1"
        << "&sdp=" + HttpManager::UrlEncode(sdpOffer);

    RTC_LOG(LS_INFO) << "pull url: " << server_addr_ << " request body:" << body.str();

    std::string url = server_addr_ + "/signaling/pull";
    HttpRequest request(url, body.str());

    XRTCGlobal::Instance()->http_manager()->Post(request, [=](HttpReply reply) {
        RTC_LOG(LS_INFO) << "signaling push response, url: " << reply.get_url()
            << ", body: " << reply.get_body()
            << ", status: " << reply.get_status_code()
            << ", err_no: " << reply.get_errno()
            << ", err_msg: " << reply.get_err_msg()
            << ", response: " << reply.get_resp();
        XRTCGlobal::Instance()->api_thread()->PostTask(webrtc::ToQueuedTask([=]() {
            handleHttpPullResponse(reply);
        }));

    }, this);

    RTC_LOG(LS_INFO) << "send webrtc pull request.....";

}

void KRTCPullImpl::OnFailure(webrtc::RTCError error) {
    if (XRTCGlobal::Instance()->engine_observer()) {
        XRTCGlobal::Instance()->engine_observer()->OnPullFailed(KRTCError::kCreateOfferErr);
    }
}

bool KRTCPullImpl::ParseReply(const HttpReply& reply, std::string& type, std::string& sdp)
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

void KRTCPullImpl::handleHttpPullResponse(const HttpReply &reply)
{
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
        XRTCGlobal::Instance()->engine_observer()->OnPullSuccess();
    }
}

void KRTCPullImpl::SendStopRequest()
{
    // https://www.charlescao92.cn/signaling/stoppull?uid=xxx&streamName=xxx
    std::stringstream body;
    body << "uid=" + uid_
        << "&streamName=" + stream_name_;

    RTC_LOG(LS_INFO) << "stop pull url: " << server_addr_ << ", request body:" << body.str();

    std::string url = server_addr_ + "/signaling/stoppull";
    HttpRequest request(url, body.str());

    XRTCGlobal::Instance()->http_manager()->Post(request, [=](HttpReply reply) {
        RTC_LOG(LS_INFO) << "signaling stoppull response, url: " << reply.get_url()
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

}  // namespace xrtc
