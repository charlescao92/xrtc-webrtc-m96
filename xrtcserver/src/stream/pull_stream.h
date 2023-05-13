/**
 * @file pull_stream.h
 * @author charles
 * @brief 
*/

#ifndef __PULL_STREAM_H_
#define __PULL_STREAM_H_

#include <stdint.h>
#include <string>

#include "stream/rtc_stream.h"
#include "pc/stream_params.h"

namespace xrtc {

class PullStream: public RtcStream {
public:
    PullStream(EventLoop *el, PortAllocator *allocator, uint64_t uid, const std::string& stream_name,
        bool audio, bool video, bool dtls_on, uint32_t log_id);
    ~PullStream();

public:
    std::string create_answer() override;
    RtcStreamType stream_type() override { return RtcStreamType::k_pull; }

    void add_audio_source(const std::vector<StreamParams>& source);
    void add_video_source(const std::vector<StreamParams>& source);
};

} // end namespace xrtc

#endif // __PULL_STREAM_H_
