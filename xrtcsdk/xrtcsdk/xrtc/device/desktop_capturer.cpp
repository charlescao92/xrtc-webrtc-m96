#include "xrtc/device/desktop_capturer.h"

#include <api/video/i420_buffer.h>
#include <api/video/video_rotation.h>
#include <rtc_base/logging.h>
#include <system_wrappers/include/sleep.h>
#include <third_party/libyuv/include/libyuv.h>

#ifdef WIN32
#include <Mmsystem.h>
#endif

#include "xrtc/base/xrtc_global.h"
#include "xrtc/media/media_frame.h"

namespace xrtc {

    static webrtc::DesktopCaptureOptions GetCaptureOption()
    {
        auto capture_options = webrtc::DesktopCaptureOptions::CreateDefault();
#if defined(_WIN32) || defined(_WIN64)
        capture_options.set_allow_directx_capturer(true);
        capture_options.set_allow_use_magnification_api(false);
#elif defined(__APPLE__)
        capture_options.set_allow_iosurface(true);
#endif
        return capture_options;
    }

    bool DesktopCapturer::GetScreenSourceList(webrtc::DesktopCapturer::SourceList& source_list)
    {
        auto desktop_capturer = webrtc::DesktopCapturer::CreateScreenCapturer(GetCaptureOption());
        return desktop_capturer->GetSourceList(&source_list);
    }

    bool DesktopCapturer::GetWindowSourceList(webrtc::DesktopCapturer::SourceList& source_list)
    {
        auto desktop_capturer = webrtc::DesktopCapturer::CreateWindowCapturer(GetCaptureOption());
        return desktop_capturer->GetSourceList(&source_list);
    }

    std::unique_ptr<DesktopCapturer> DesktopCapturer::Create(SourceId source_id, size_t out_width, size_t out_height, size_t target_fps)
    {
        std::unique_ptr<DesktopCapturer> screen_capture(new DesktopCapturer());
        if (!screen_capture->Init(source_id, out_width, out_height, target_fps)) {
            RTC_LOG(LS_WARNING) << "Failed to create DesktopCapturer(w = " << out_width
                << ", h = " << out_height << ", fps = " << target_fps
                << ")";
            return nullptr;
        }
        return screen_capture;
    }

    DesktopCapturer::DesktopCapturer()
    {
        capture_options_ = webrtc::DesktopCaptureOptions::CreateDefault();
#if defined(_WIN32) || defined(_WIN64)
        capture_options_.set_allow_directx_capturer(true);
        capture_options_.set_allow_use_magnification_api(false);
#elif defined(__APPLE__)
        capture_options_.set_allow_iosurface(true);
#endif
    }

    DesktopCapturer::~DesktopCapturer()
    {
        Destroy();
    }

    void DesktopCapturer::SetFrameCallback(const FrameCallback& frame_callback)
    {
        frame_callback_ = frame_callback;
    }

    bool DesktopCapturer::Init(webrtc::DesktopCapturer::SourceId source_id, size_t out_width, size_t out_height, size_t target_fps)
    {
        if (is_capturing_) {
            return false;
        }

        if (target_fps == 0) {
            return false;
        }

        out_width_ = out_width;
        out_height_ = out_height;
        target_fps_ = target_fps;

        if (!CreateCapture(source_id)) {
            RTC_LOG(LS_WARNING) << "Failed to found capturer.";
            return false;
        }

        if (is_capture_cursor_) {
            desktop_capturer_.reset(new webrtc::DesktopAndCursorComposer(std::move(desktop_capturer_), GetCaptureOption()));
        }

        if (!desktop_capturer_->SelectSource(source_id_)) {
            RTC_LOG(LS_WARNING) << "Failed to select source.";
            return false;
        }

        return true;
    }

    void DesktopCapturer::Start()
    {
        is_capturing_ = true;
        capture_thread_.reset(new std::thread([this] {
            CaptureThread();
        }));

        KRTCError err = KRTCError::kNoErr;
        // 回调启动结果
        if (err != KRTCError::kNoErr) {
            if (XRTCGlobal::Instance()->engine_observer()) {
                XRTCGlobal::Instance()->engine_observer()->OnVideoSourceFailed(err);
            }
        }
        else {
            if (XRTCGlobal::Instance()->engine_observer()) {
                XRTCGlobal::Instance()->engine_observer()->OnVideoSourceSuccess();
            }
        }
    }

    void DesktopCapturer::Stop()
    {
        Destroy();
    }

    void DesktopCapturer::Destroy()
    {
        if (is_capturing_) {
            is_capturing_ = false;
            capture_thread_->join();
            capture_thread_.reset();
        }

        if (desktop_capturer_) {
            desktop_capturer_.reset();
        }
    }

    bool DesktopCapturer::CreateCapture(webrtc::DesktopCapturer::SourceId source_id)
    {
        bool is_found = false;
        webrtc::DesktopCapturer::SourceList source_list;
        webrtc::DesktopCaptureOptions options = GetCaptureOption();

        source_list.clear();
        desktop_capturer_ = webrtc::DesktopCapturer::CreateScreenCapturer(options);
        if (desktop_capturer_->GetSourceList(&source_list)) {
            for (auto iter : source_list) {
                if (source_id == iter.id) {
                    source_id_ = source_id;
                    title_ = iter.title;
                    is_found = true;
                    break;
                }
            }
        }

        if (is_found) {
            return true;
        }

        source_list.clear();
        desktop_capturer_ = webrtc::DesktopCapturer::CreateWindowCapturer(options);
        if (desktop_capturer_->GetSourceList(&source_list)) {
            for (auto iter : source_list) {
                if (source_id == iter.id) {
                    source_id_ = source_id;
                    title_ = iter.title;
                    is_found = true;
                    break;
                }
            }
        }

        if (!is_found) {
            desktop_capturer_.reset();
        }

        return is_found;
    }

    void DesktopCapturer::CaptureThread()
    {
        int64_t last_ts = 0;
        int64_t frame_duration_ms = 1000 / target_fps_;

        desktop_capturer_->Start(this);

        while (is_capturing_) {
            int64_t now_ts = rtc::TimeMillis();
            if (last_ts + frame_duration_ms <= now_ts) {
                last_ts = now_ts;
                desktop_capturer_->CaptureFrame();
            }

            int64_t delta_time_ms = frame_duration_ms - (rtc::TimeMillis() - now_ts);
            if (delta_time_ms <= 0) {
                delta_time_ms = 1;
            }

            if (delta_time_ms > 0) {
#ifdef WIN32
                timeBeginPeriod(1);
                Sleep(static_cast<int>(delta_time_ms));
                timeEndPeriod(1);
#else 
                webrtc::SleepMs(delta_time_ms);
#endif
            }
        }
    }

    void DesktopCapturer::OnCaptureResult(webrtc::DesktopCapturer::Result result, std::unique_ptr<webrtc::DesktopFrame> frame)
    {
        if (result != webrtc::DesktopCapturer::Result::SUCCESS) {
            RTC_LOG(LS_ERROR) << "Capture frame faiiled, result: " << result;
            return;
        }

        int width = frame->size().width();
        int height = frame->size().height();
        rtc::scoped_refptr<webrtc::I420Buffer> i420_buffer = webrtc::I420Buffer::Create(width, height);

        libyuv::ConvertToI420(frame->data(), 0, i420_buffer->MutableDataY(),
            i420_buffer->StrideY(), i420_buffer->MutableDataU(),
            i420_buffer->StrideU(), i420_buffer->MutableDataV(),
            i420_buffer->StrideV(), 0, 0, width, height,
            i420_buffer->width(), i420_buffer->height(),
            libyuv::kRotate0, libyuv::FOURCC_ARGB);

        auto video_frame = webrtc::VideoFrame(i420_buffer, webrtc::kVideoRotation_0, rtc::TimeMicros());
        if (frame_callback_) {
            frame_callback_(video_frame);
        }

        OnFrame(video_frame);
    }

    void DesktopCapturer::OnFrame(const webrtc::VideoFrame& frame) {
       VideoCapturer::OnFrame(frame);
    }

}
