/**
 * @file rtc_stream_manager.h
 * @author charles
 * @brief 
*/

#ifndef __RTC_STREAM_MANAGER_H_
#define __RTC_STREAM_MANAGER_H_

#include <stdint.h>
#include <string>
#include <unordered_map>
#include <memory>

#include <rtc_base/rtc_certificate.h>

#include "stream/rtc_stream.h"
#include "xrtcserver_def.h"

namespace xrtc {

class EventLoop;
class PushStream;
class PortAllocator;
class PullStream;

class RtcStreamManager : public RtcStreamListener {
public:
    RtcStreamManager(EventLoop *el);
    ~RtcStreamManager();

public:
    int create_push_stream(const std::shared_ptr<RtcMsg>& msg, std::string& answer);
    int create_pull_stream(const std::shared_ptr<RtcMsg>& msg, std::string& answer);

    int stop_push(uint64_t uid, const std::string& stream_name);
    int stop_pull(uint64_t uid, const std::string& stream_name);

    void on_connection_state(RtcStream* stream, PeerConnectionState state) override;
    void on_rtp_packet_received(RtcStream* stream, const char* data, size_t len) override;
    void on_rtcp_packet_received(RtcStream* stream, const char* data, size_t len) override;
    void on_stream_exception(RtcStream* stream);
    
private:
    PushStream *_find_push_stream(const std::string& stram_name);
    void _remove_push_stream(RtcStream* stream);
    void _remove_push_stream(uint64_t uid, const std::string& stream_name);
    PullStream *_find_pull_stream(const std::string& stram_name);
    void _remove_pull_stream(RtcStream* stream);
    void _remove_pull_stream(uint64_t uid, const std::string& stream_name);

private:
    EventLoop *el_;
    std::unordered_map<std::string, PushStream*> push_streams_;
    std::unordered_map<std::string, PullStream*> pull_streams_;
    std::unique_ptr<PortAllocator> port_allocator_;
};

} // end namespace xrtc

#endif // __RTC_STREAM_MANAGER_H_
