// H:\webrtc\webrtc-checkout\src\test\video_renderer.h

#ifndef KRTCSDK_KRTC_VIDEO_RENDERER_H_
#define KRTCSDK_KRTC_VIDEO_RENDERER_H_

#include <stddef.h> 

#include <api/media_stream_interface.h>
#include <api/video/video_sink_interface.h>
#include <api/video/video_frame.h>

#include "xrtc/xrtc.h"

namespace xrtc {

 class VideoRenderer : public rtc::VideoSinkInterface<webrtc::VideoFrame> {
 public:
    // Creates a platform-specific renderer if possible, or a null implementation
    // if failing.
    static std::unique_ptr<VideoRenderer> Create(
        CONTROL_TYPE type,
        int hwnd,
        size_t width,
        size_t height);

    // Returns a renderer rendering to a platform specific window if possible,
    // NULL if none can be created.
    // Creates a platform-specific renderer if possible, returns NULL if a
    // platform renderer could not be created. This occurs, for instance, when
    // running without an X environment on Linux.
    static std::unique_ptr<VideoRenderer> CreatePlatformRenderer(
        int hwnd,
        size_t width,
        size_t height);

    virtual ~VideoRenderer() {}

protected:
    VideoRenderer() {}

 };
}  // namespace xrtc

#endif  // KRTCSDK_KRTC_VIDEO_RENDERER_H_
