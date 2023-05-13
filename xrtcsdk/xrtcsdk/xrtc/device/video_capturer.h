#ifndef KRTCSDK_KRTC_DEVICE_VIDEO_CAPTURER_H_
#define KRTCSDK_KRTC_DEVICE_VIDEO_CAPTURER_H_

#include <stddef.h>

#include <memory>
#include <atomic>

#include <api/video/video_frame.h>
#include <api/video/video_source_interface.h>
#include <media/base/video_adapter.h>
#include <media/base/video_broadcaster.h>
#include <rtc_base/synchronization/mutex.h>

namespace xrtc {

class VideoCapturer : public rtc::VideoSourceInterface<webrtc::VideoFrame> {
public:
    class FramePreprocessor {
    public:
        virtual ~FramePreprocessor() = default;

        virtual webrtc::VideoFrame Preprocess(const webrtc::VideoFrame& frame) = 0;
    };

    ~VideoCapturer() override;

    void AddOrUpdateSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink,
        const rtc::VideoSinkWants& wants) override;
    void RemoveSink(rtc::VideoSinkInterface<webrtc::VideoFrame>* sink) override;
    void SetFramePreprocessor(std::unique_ptr<FramePreprocessor> preprocessor) {
        webrtc::MutexLock lock(&lock_);
        preprocessor_ = std::move(preprocessor);
    }

protected:
    void OnFrame(const webrtc::VideoFrame& frame);
    
    rtc::VideoSinkWants GetSinkWants();

private:
    void CalcFps(const webrtc::VideoFrame& frame);

    void UpdateVideoAdapter();
    webrtc::VideoFrame MaybePreprocess(const webrtc::VideoFrame& frame);

    webrtc::Mutex lock_;
    std::unique_ptr<FramePreprocessor> preprocessor_ RTC_GUARDED_BY(lock_);
    rtc::VideoBroadcaster broadcaster_;
    cricket::VideoAdapter video_adapter_;

    std::atomic<int> fps_{ 0 };
    std::atomic<int64_t> last_frame_ts_{ 0 };
    std::atomic<int64_t> start_time_{ 0 };
};

}  // namespace xrtc

#endif  // KRTCSDK_KRTC_DEVICE_VIDEO_CAPTURER_H_
