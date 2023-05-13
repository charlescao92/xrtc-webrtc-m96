#include <rtc_base/logging.h>

#include "base/event_loop.h"
#include "ice/port_allocator.h"
#include "stream/rtc_stream_manager.h"
#include "stream/push_stream.h"
#include "stream/pull_stream.h"
#include "server/settings.h"

namespace xrtc {
  
RtcStreamManager::RtcStreamManager(EventLoop *el) : 
    el_(el), 
    port_allocator_(new PortAllocator) 
{
    port_allocator_->set_port_range(Singleton<Settings>::Instance()->IceMinPort(), Singleton<Settings>::Instance()->IceMaxPort());
}

RtcStreamManager::~RtcStreamManager() {
}

int RtcStreamManager::create_push_stream(const std::shared_ptr<RtcMsg>& msg, std::string& answer) {
    PushStream* stream = _find_push_stream(msg->stream_name);
    if (stream) {
        push_streams_.erase(msg->stream_name);
        delete stream;
        stream = nullptr;
    }

    stream = new PushStream(el_, port_allocator_.get(), msg->uid, msg->stream_name,
            msg->audio, msg->video, msg->dtls_on, msg->log_id);
    stream->register_listener(this);
    stream->start((rtc::RTCCertificate*)msg->certificate);

    stream->set_remote_sdp(msg->sdp);

    answer = stream->create_answer();

    RTC_LOG(LS_INFO) << "add push stream, uid: " << msg->uid
                << ", stream_name: " << msg->stream_name
                << ", log_id: " << msg->log_id;

    push_streams_[msg->stream_name] = stream;

    return 0;
}

int RtcStreamManager::create_pull_stream(const std::shared_ptr<RtcMsg>& msg, std::string& answer) {
    PushStream* push_stream = _find_push_stream(msg->stream_name);
    if (!push_stream) {
        RTC_LOG(LS_WARNING) << "Stream not found, uid: " << msg->uid << ", stream_name: "
            << msg->stream_name << ", log_id: " << msg->log_id;
        return -1;
    }

    _remove_pull_stream(msg->uid, msg->stream_name);
    std::vector<StreamParams> audio_source;
    std::vector<StreamParams> video_source;
    push_stream->get_audio_source(audio_source);
    push_stream->get_video_source(video_source);

    PullStream *stream = new PullStream(el_, port_allocator_.get(), msg->uid, msg->stream_name,
            msg->audio, msg->video, msg->dtls_on, msg->log_id);
    stream->register_listener(this);
    stream->add_audio_source(audio_source);
    stream->add_video_source(video_source);
    stream->start((rtc::RTCCertificate*)msg->certificate);

    stream->set_remote_sdp(msg->sdp);
    
    answer = stream->create_answer();

    RTC_LOG(LS_WARNING) << "add push stream, uid: " << msg->uid
                << ", stream_name: " << msg->stream_name
                << ", log_id: " << msg->log_id;

    pull_streams_[msg->stream_name] = stream;

    return 0;
}

PushStream *RtcStreamManager::_find_push_stream(const std::string& stram_name) {
    auto iter = push_streams_.find(stram_name);
    if (iter != push_streams_.end()) {
        return iter->second;
    }
    return nullptr;
}

void RtcStreamManager::_remove_push_stream(RtcStream* stream) {
    if (!stream) {
        return;
    }

    _remove_push_stream(stream->get_uid(), stream->get_stream_name());
}

void RtcStreamManager::_remove_push_stream(uint64_t uid, const std::string& stream_name) {
    PushStream* push_stream = _find_push_stream(stream_name);
    if (push_stream && uid == push_stream->get_uid()) {
        push_streams_.erase(stream_name);
        delete push_stream;
    }
}

int RtcStreamManager::stop_push(uint64_t uid, const std::string& stream_name) {
    _remove_push_stream(uid, stream_name);
    return 0;
}

int RtcStreamManager::stop_pull(uint64_t uid, const std::string& stream_name) {
    _remove_pull_stream(uid, stream_name);
    return 0;
}

PullStream *RtcStreamManager::_find_pull_stream(const std::string& stram_name) {
    auto iter = pull_streams_.find(stram_name);
    if (iter != pull_streams_.end()) {
        return iter->second;
    }
    return nullptr;
}

void RtcStreamManager::_remove_pull_stream(RtcStream* stream) {
    if (!stream) {
        return;
    }

    _remove_pull_stream(stream->get_uid(), stream->get_stream_name());
}

void RtcStreamManager::_remove_pull_stream(uint64_t uid, const std::string& stream_name) {
    PullStream* pull_stream = _find_pull_stream(stream_name);
    if (pull_stream && uid == pull_stream->get_uid()) {
        pull_streams_.erase(stream_name);
        delete pull_stream;
    }
}

void RtcStreamManager::on_connection_state(RtcStream* stream, PeerConnectionState state) {
    if (state == PeerConnectionState::k_failed) {
        if (stream->stream_type() == RtcStreamType::k_push) {
            _remove_push_stream(stream);
        } else if (stream->stream_type() == RtcStreamType::k_pull) {
            _remove_pull_stream(stream);
        }
    } 
}

void RtcStreamManager::on_rtp_packet_received(RtcStream* stream, const char* data, size_t len) {
    if (RtcStreamType::k_push == stream->stream_type()) {
        PullStream* pull_stream = _find_pull_stream(stream->get_stream_name());
        if (pull_stream) {
            pull_stream->send_rtp(data, len);
        }
    }
}

void RtcStreamManager::on_rtcp_packet_received(RtcStream* stream,  const char* data, size_t len) {
    if (RtcStreamType::k_push == stream->stream_type()) {
        PullStream* pull_stream = _find_pull_stream(stream->get_stream_name());
        if (pull_stream) {
            pull_stream->send_rtcp(data, len);
        }
    } else if (RtcStreamType::k_pull == stream->stream_type()) {
        PushStream* push_stream = _find_push_stream(stream->get_stream_name());
        if (push_stream) {
            push_stream->send_rtcp(data, len);
        }
    }
}

void RtcStreamManager::on_stream_exception(RtcStream* stream) {
    if (RtcStreamType::k_push == stream->stream_type()) {
        _remove_push_stream(stream);
    } else if (RtcStreamType::k_pull == stream->stream_type()) {
        _remove_pull_stream(stream);
    }
}

} // end namespace xrtc
