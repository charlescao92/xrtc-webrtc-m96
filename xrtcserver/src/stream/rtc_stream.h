/**
 * @file rtc_stream_manager.h
 * @author charles
 * @brief 
*/

#ifndef __RTC_STREAM_H_
#define __RTC_STREAM_H_

#include <stdint.h>
#include <string>
#include <memory>

#include <rtc_base/rtc_certificate.h>
#include <rtc_base/third_party/sigslot/sigslot.h>

#include "pc/peer_connection.h"

namespace xrtc {

class EventLoop;
class PortAllocator;
class RtcStream;

enum class RtcStreamType {
    k_push,
    k_pull
};

class RtcStreamListener {
public:
    virtual void on_connection_state(RtcStream* stream, PeerConnectionState state) = 0;
    virtual void on_rtp_packet_received(RtcStream* stream, const char* data, size_t len) = 0;
    virtual void on_rtcp_packet_received(RtcStream* stream, const char* data, size_t len) = 0;
    virtual void on_stream_exception(RtcStream* stream) = 0;
};

class RtcStream : public sigslot::has_slots<> {
public:
    RtcStream(EventLoop *el, PortAllocator *allocator, uint64_t uid, const std::string& stream_name,
        bool audio, bool video, bool dtls_on, uint32_t log_id);
    virtual ~RtcStream();

public:
    virtual std::string create_answer() = 0;
    virtual RtcStreamType stream_type() = 0;

public:
    int start(rtc::RTCCertificate* certificate);
    int set_remote_sdp(const std::string& sdp);
    void register_listener(RtcStreamListener* listener) { listener_ = listener; }

    uint64_t get_uid() { return uid; }
    const std::string& get_stream_name() { return stream_name; }

    int send_rtp(const char* data, size_t len);
    int send_rtcp(const char* data, size_t len);

    std::string to_string();

private:
    void _on_connection_state(PeerConnection* pc, PeerConnectionState state);
    void _on_rtp_packet_received(PeerConnection*, rtc::CopyOnWriteBuffer* packet, int64_t ts);
    void _on_rtcp_packet_received(PeerConnection*, rtc::CopyOnWriteBuffer* packet, int64_t ts);

protected:
    EventLoop *el;
    uint64_t uid;
    const std::string stream_name;
    bool audio;
    bool video;
    bool dtls_on;
    uint32_t log_id; 

    PeerConnection *pc;
    PeerConnectionState state_ = PeerConnectionState::k_new;
    RtcStreamListener *listener_ = nullptr;
    TimerWatcher *ice_timeout_watcher_ = nullptr;

    friend class RtcStreamManager;
    friend void ice_timeout_cb(EventLoop* el, TimerWatcher* w, void* data);
};

} // end namespace xrtc

#endif // __RTC_STREAM_H_
