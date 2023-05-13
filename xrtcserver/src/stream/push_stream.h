/**
 * @file push_stream.h
 * @author charles
 * @brief 
*/

#ifndef __PUSH_STREAM_H_
#define __PUSH_STREAM_H_

#include <stdint.h>
#include <string>

#include "stream/rtc_stream.h"
#include "pc/stream_params.h"

namespace xrtc {

class PushStream: public RtcStream {
public:
    PushStream(EventLoop *el, PortAllocator *allocator, uint64_t uid, const std::string& stream_name,
        bool audio, bool video, bool dtls_on, uint32_t log_id);
    ~PushStream();

public:
    std::string create_answer() override;
    RtcStreamType stream_type() override { return RtcStreamType::k_push; }

    bool get_audio_source(std::vector<StreamParams>& source);
    bool get_video_source(std::vector<StreamParams>& source);

private:
    bool _get_source(const std::string& mid, std::vector<StreamParams>& source);

private:
    bool dtls_on_ = true;
};

} // end namespace xrtc

#endif // __PUSH_STREAM_H_
