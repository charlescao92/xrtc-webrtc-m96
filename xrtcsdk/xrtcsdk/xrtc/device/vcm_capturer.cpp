#include "xrtc/device/vcm_capturer.h"

#include <stdint.h>

#include <memory>

#include <modules/video_capture/video_capture_factory.h>
#include <rtc_base/checks.h>
#include <rtc_base/logging.h>
#include <rtc_base/task_utils/to_queued_task.h>

#include "xrtc/base/xrtc_global.h"
#include "xrtc/media/media_frame.h"

namespace xrtc {

std::unique_ptr<VcmCapturer> VcmCapturer::Create(const std::string& cam_id,  size_t width, size_t height,
                                                size_t target_fps)
{
    std::unique_ptr<VcmCapturer> vcm_capturer(new VcmCapturer(cam_id, width, height, target_fps));
    return std::move(vcm_capturer);
}

VcmCapturer::VcmCapturer(const std::string& cam_id, size_t width, size_t height, size_t target_fps) :
    cam_id_(cam_id),
    vcm_(nullptr),
    signaling_thread_(rtc::Thread::Current()),
    device_info_(XRTCGlobal::Instance()->video_device_info())
{
    capability_.width = static_cast<int32_t>(width);
    capability_.height = static_cast<int32_t>(height);
    capability_.maxFPS = static_cast<int32_t>(target_fps);
    capability_.videoType = webrtc::VideoType::kI420;
}

void VcmCapturer::Start()
{
    RTC_LOG(LS_INFO) << "CamImpl Start call";
    signaling_thread_->PostTask(webrtc::ToQueuedTask([=] {
        RTC_LOG(LS_INFO) << "CamImpl Start PostTask";

        KRTCError err = KRTCError::kNoErr;
        do {
            if (has_start_) {
                RTC_LOG(LS_WARNING) << "CamImpl already start, ignore";
                break;
            }

            vcm_ = webrtc::VideoCaptureFactory::Create(cam_id_.c_str());
            if (!vcm_) {
                err = KRTCError::kVideoCreateCaptureErr;
                break;
            }

            if (device_info_->NumberOfCapabilities(cam_id_.c_str()) <= 0) {
                err = KRTCError::kVideoNoCapabilitiesErr;
                RTC_LOG(LS_WARNING) << "CamImpl no capabilities";
                break;
            }

            webrtc::VideoCaptureCapability best_cap;

            if (device_info_->GetBestMatchedCapability(cam_id_.c_str(),
                capability_, best_cap) < 0)
            {
                err = KRTCError::kVideoNoBestCapabilitiesErr;
                RTC_LOG(LS_WARNING) << "CamImpl no best capabilities";
                break;
            }

            vcm_->RegisterCaptureDataCallback(this);

            if (vcm_->StartCapture(best_cap) < 0) {
                err = KRTCError::kVideoStartCaptureErr;
                RTC_LOG(LS_WARNING) << "CamImpl video StartCapture error";
                break;
            }

            if (vcm_->CaptureStarted()) {
                has_start_ = true;
            }

        } while (0);

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
    }));
}

void VcmCapturer::Stop() {
    if (has_start_) {
        Destroy();
        has_start_ = false;
    }
}

void VcmCapturer::Destroy() {
    if (!has_start_) {
        return;
    }

    if (!vcm_)
        return;

    vcm_->StopCapture();
    vcm_->DeRegisterCaptureDataCallback();
    vcm_ = nullptr;
}

VcmCapturer::~VcmCapturer() {
    Destroy();
}

void VcmCapturer::OnFrame(const webrtc::VideoFrame& frame) {
    VideoCapturer::OnFrame(frame);
}

} // namespace xrtc
