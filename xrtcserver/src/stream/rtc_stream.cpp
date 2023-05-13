#include <rtc_base/logging.h>

#include "stream/rtc_stream.h"
#include "ice/port_allocator.h"

namespace xrtc {

const size_t k_ice_timeout = 30000; // 30s

RtcStream::RtcStream(EventLoop *el, PortAllocator *allocator, uint64_t uid, const std::string& stream_name,
    bool audio, bool video, bool dtls_on, uint32_t log_id) :
    el(el), 
    uid(uid),
    stream_name(stream_name),
    audio(audio),
    video(video),
    dtls_on(dtls_on),
    log_id(log_id),
    pc(new PeerConnection(el, allocator, dtls_on)) 
{
    pc->signal_connection_state.connect(this, &RtcStream::_on_connection_state);
    pc->signal_rtp_packet_received.connect(this, &RtcStream::_on_rtp_packet_received);
    pc->signal_rtcp_packet_received.connect(this, &RtcStream::_on_rtcp_packet_received);
}

RtcStream::~RtcStream() {
    if (ice_timeout_watcher_) {
        el->delete_timer(ice_timeout_watcher_);
        ice_timeout_watcher_ = nullptr;
    }

    pc->destroy();
}

void RtcStream::_on_connection_state(PeerConnection* /*pc*/, PeerConnectionState state) {
    if (state_ == state) {
        return;
    }

    RTC_LOG(LS_INFO) << to_string() << ": PeerConnectionState change from " << state_ << " to " << state;

    state_ = state;

    if (state_ == PeerConnectionState::k_connected) {
        if (ice_timeout_watcher_) {
            el->delete_timer(ice_timeout_watcher_);
            ice_timeout_watcher_ = nullptr;
        }
    }

    if (listener_) {
        listener_->on_connection_state(this, state);
    }
}

void RtcStream::_on_rtp_packet_received(PeerConnection*, 
        rtc::CopyOnWriteBuffer* packet, int64_t /*ts*/)
{
    if (listener_) {
        listener_->on_rtp_packet_received(this, (const char*)packet->data(), packet->size());
    }
}

void RtcStream::_on_rtcp_packet_received(PeerConnection*, 
        rtc::CopyOnWriteBuffer* packet, int64_t /*ts*/)
{
    if (listener_) {
        listener_->on_rtcp_packet_received(this, (const char*)packet->data(), packet->size());
    }
}

void ice_timeout_cb(EventLoop* /*el*/, TimerWatcher* /*w*/, void* data) {
    RtcStream* stream = (RtcStream*)data;
    if (stream->state_ != PeerConnectionState::k_connected) {
        if (stream->listener_) {
            stream->listener_->on_stream_exception(stream);
        }
    }
}

int RtcStream::start(rtc::RTCCertificate* certificate) {
    ice_timeout_watcher_ = el->create_timer(ice_timeout_cb, this, false);
    el->start_timer(ice_timeout_watcher_, k_ice_timeout * 1000);

    return pc->init(certificate);
}

int RtcStream::set_remote_sdp(const std::string& sdp) {
    return pc->set_remote_sdp(sdp);
}

int RtcStream::send_rtp(const char* data, size_t len) {
    if (pc) {
        return pc->send_rtp(data, len);
    }
    return -1;
}

int RtcStream::send_rtcp(const char* data, size_t len) {
    if (pc) {
        return pc->send_rtcp(data, len);
    }
    return -1;
}

std::string RtcStream::to_string() {
    std::stringstream ss;
    ss << "Stream[" << this << "|" << uid << "|" << stream_name << "]";
    return ss.str();
}

}