#include <modules/video_capture/video_capture_factory.h>
#include <rtc_base/task_utils/to_queued_task.h>
#include <media/engine/adm_helpers.h>
#include <api/create_peerconnection_factory.h>
#include <api/audio_codecs/builtin_audio_decoder_factory.h>
#include <api/audio_codecs/builtin_audio_encoder_factory.h>
#include <api/video_codecs/builtin_video_decoder_factory.h>
#include <api/video_codecs/builtin_video_encoder_factory.h>

#include "xrtc/base/xrtc_global.h"
#include "xrtc/base/xrtc_http.h"

#if defined(_WIN32) || defined(_WIN64)
#include "xrtc/codec/external_video_encoder_factory.h"
#endif

namespace xrtc {

XRTCGlobal* XRTCGlobal::Instance() {
    static XRTCGlobal* const instance = new XRTCGlobal();
    return instance;
}

XRTCGlobal::XRTCGlobal() :
    signaling_thread_(rtc::Thread::Create()),
    worker_thread_(rtc::Thread::Create()),
    network_thread_(rtc::Thread::CreateWithSocketServer()),
    video_device_info_(webrtc::VideoCaptureFactory::CreateDeviceInfo()),
    task_queue_factory_(webrtc::CreateDefaultTaskQueueFactory())
{
    signaling_thread_->SetName("signaling_thread", nullptr);
    signaling_thread_->Start();

    worker_thread_->SetName("worker_thread", nullptr);
    worker_thread_->Start();

    network_thread_->SetName("network_thread", nullptr);
    network_thread_->Start();

    http_manager_ = new HttpManager();
    http_manager_->Start();

    worker_thread_->PostTask(webrtc::ToQueuedTask([=]() {
        audio_device_ = webrtc::AudioDeviceModule::Create(
            webrtc::AudioDeviceModule::kPlatformDefaultAudio,
            task_queue_factory_.get());
        audio_device_->Init();
    }));

    DesktopCapturer::GetScreenSourceList(screen_source_list_);
}

XRTCGlobal::~XRTCGlobal() {}

webrtc::PeerConnectionFactoryInterface* XRTCGlobal::push_peer_connection_factory()
{
#if defined(_WIN32) || defined(_WIN64)
    push_peer_connection_factory_ = webrtc::CreatePeerConnectionFactory(
        network_thread_.get(), /* network_thread */
        worker_thread_.get(), /* worker_thread */
        signaling_thread_.get(),  /* signaling_thread */
        audio_device_,  /* default_adm */
        webrtc::CreateBuiltinAudioEncoderFactory(),
        webrtc::CreateBuiltinAudioDecoderFactory(),
        xrtc::CreateBuiltinExternalVideoEncoderFactory(),// webrtc::CreateBuiltinVideoEncoderFactory(),
        webrtc::CreateBuiltinVideoDecoderFactory(),
        nullptr, /* audio_mixer */
        nullptr, /* audio_processing */
        nullptr, /*audio_frame_processor*/
        std::move(task_queue_factory_));
#else
    push_peer_connection_factory_ = webrtc::CreatePeerConnectionFactory(
        network_thread_.get(), /* network_thread */
        worker_thread_.get(), /* worker_thread */
        signaling_thread_.get(),  /* signaling_thread */
        audio_device_,  /* default_adm */
        webrtc::CreateBuiltinAudioEncoderFactory(),
        webrtc::CreateBuiltinAudioDecoderFactory(),
        webrtc::CreateBuiltinVideoEncoderFactory(),
        webrtc::CreateBuiltinVideoDecoderFactory(),
        nullptr, /* audio_mixer */
        nullptr, /* audio_processing */
        nullptr, /*audio_frame_processor*/
        std::move(task_queue_factory_));
#endif

    return push_peer_connection_factory_.get();
}

void XRTCGlobal::CreateVcmCapturerSource(const char* cam_id)
{
    signaling_thread_->PostTask(webrtc::ToQueuedTask([=]() {
        camera_capturer_source_ = VcmCapturerTrackSource::Create(cam_id);
        if (!camera_capturer_source_) {
            if (XRTCGlobal::Instance()->engine_observer()) {
                XRTCGlobal::Instance()->engine_observer()->OnPreviewFailed(KRTCError::kVideoCreateCaptureErr);
            }
            return;
        }

        SetCurrentCaptureType(CAPTURE_TYPE::CAMERA);
    }));
}

void XRTCGlobal::StartVcmCapturerSource()
{
    signaling_thread_->PostTask(webrtc::ToQueuedTask([=]() {
        if (camera_capturer_source_) {
            camera_capturer_source_->Start();
        }

        if (XRTCGlobal::Instance()->engine_observer()) {
            XRTCGlobal::Instance()->engine_observer()->OnPreviewSuccess();
        }
    }));
}

void XRTCGlobal::StopVcmCapturerSource()
{
    signaling_thread_->PostTask(webrtc::ToQueuedTask([=]() {
        if (camera_capturer_source_) {
            camera_capturer_source_->Stop();
        }
    }));
}

void XRTCGlobal::CreateDesktopCapturerSource(uint16_t screen_index, uint16_t target_fps)
{
    signaling_thread_->PostTask(webrtc::ToQueuedTask([=]() {
        desktop_capturer_source_ = DesktopCapturerTrackSource::Create(screen_index, target_fps);
        if (!desktop_capturer_source_) {
            if (XRTCGlobal::Instance()->engine_observer()) {
                XRTCGlobal::Instance()->engine_observer()->OnPreviewFailed(KRTCError::kVideoCreateCaptureErr);     
            }
            return;
        }

        SetCurrentCaptureType(CAPTURE_TYPE::SCREEN);
    }));
}

void XRTCGlobal::StartDesktopCapturerSource()
{
    signaling_thread_->PostTask(webrtc::ToQueuedTask([=]() {
        if (desktop_capturer_source_) {
            desktop_capturer_source_->Start();

            if (XRTCGlobal::Instance()->engine_observer()) {
                XRTCGlobal::Instance()->engine_observer()->OnPreviewSuccess();
            }
        }
    }));
}

void XRTCGlobal::StopDesktopCapturerSource()
{
    signaling_thread_->PostTask(webrtc::ToQueuedTask([=]() {
        if (desktop_capturer_source_) {
            desktop_capturer_source_->Stop();
        }
    }));
}

webrtc::VideoTrackSource* XRTCGlobal::current_video_source()
{
    switch (current_capture_type_) {
    case CAPTURE_TYPE::CAMERA:
        return camera_capturer_source_.get();
    case CAPTURE_TYPE::SCREEN:
        return desktop_capturer_source_.get();
    default:
        return nullptr;
    }
}

} // namespace xrtc
