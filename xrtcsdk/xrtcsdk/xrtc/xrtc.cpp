#include <map>

#include <rtc_base/logging.h>
#include <rtc_base/task_utils/to_queued_task.h>

#include "xrtc/xrtc.h"
#include "xrtc/base/xrtc_global.h"
#include "xrtc/media/xrtc_pusher.h"
#include "xrtc/media/xrtc_puller.h"
#include "xrtc/media/xrtc_preview.h"
#include "xrtc/device/camera_video_source.h"
#include "xrtc/device/desktop_video_source.h"
#include "xrtc/device/mic_impl.h"

namespace xrtc {

typedef std::map<const KRTCError, std::string> KRTCErrStrs;

KRTCErrStrs KRTCErrStr = {
    {KRTCError::kNoErr, "NoErr"},
    {KRTCError::kVideoCreateCaptureErr,         "VideoCreateCaptureErr"},
    {KRTCError::kVideoNoCapabilitiesErr,        "VideoNoCapabilitiesErr"},
    {KRTCError::kVideoNoBestCapabilitiesErr,    "VideoNoBestCapabilitiesErr"},
    {KRTCError::kVideoStartCaptureErr,          "VideoStartCaptureErr"},
    {KRTCError::kPreviewNoVideoSourceErr,       "PreviewNoVideoSourceErr"},
    {KRTCError::kInvalidUrlErr,                 "InvalidUrlErr"},
    {KRTCError::kAddTrackErr,                   "AddTrackErr"},
    {KRTCError::kCreateOfferErr,                "CreateOfferErr"},
    {KRTCError::kSendOfferErr,                  "SendOfferErr"},
    {KRTCError::kParseAnswerErr,                "ParseAnswerErr"},
    {KRTCError::kAnswerResponseErr,             "kAnswerResponseErr"},
    {KRTCError::kNoAudioDeviceErr,              "NoAudioDeviceErr"},
    {KRTCError::kAudioNotFoundErr,              "AudioNotFoundErr"},
    {KRTCError::kAudioSetRecordingDeviceErr,    "AudioSetRecordingDeviceErr"},
    {KRTCError::kAudioInitRecordingErr,	        "AudioInitRecordingErr"},
    {KRTCError::kAudioStartRecordingErr,        "AudioStartRecordingErr"},
};

void KRTCEngine::Init(KRTCEngineObserver* observer) {
    rtc::LogMessage::LogTimestamps(true);
    rtc::LogMessage::LogThreads(true);
    rtc::LogMessage::LogToDebug(rtc::LS_VERBOSE);

    XRTCGlobal::Instance()->RegisterEngineObserver(observer);
}

const char* KRTCEngine::GetErrString(const KRTCError& err) {
    return KRTCErrStr[err].c_str();
}

uint32_t KRTCEngine::GetCameraCount() {
    return XRTCGlobal::Instance()->api_thread()->Invoke<uint32_t>(RTC_FROM_HERE, [=]() {
        if (!XRTCGlobal::Instance()->video_device_info()) {
            return (uint32_t)0;
        }
        return XRTCGlobal::Instance()->video_device_info()->NumberOfDevices();
    });
}

int32_t KRTCEngine::GetCameraInfo(int index, char* device_name, uint32_t device_name_length,
    char* device_id, uint32_t device_id_length)
{
    assert(device_name_length == 128 && device_id_length == 128);
    return XRTCGlobal::Instance()->api_thread()->Invoke<int32_t>(RTC_FROM_HERE, [&]() {
        int32_t ret = -1;
        if (!XRTCGlobal::Instance()->video_device_info()) {
            return ret;
        }
        ret = XRTCGlobal::Instance()->video_device_info()->GetDeviceName(index,
            device_name, device_name_length, device_id, device_id_length);
        return ret;
    });
}

IVideoHandler* KRTCEngine::CreateCameraSource(const char* cam_id) {
    return XRTCGlobal::Instance()->api_thread()->Invoke<IVideoHandler*>(RTC_FROM_HERE, [=]() {
        return new CameraVideoSource(cam_id);
    });
}

uint32_t KRTCEngine::GetScreenCount()
{
    return XRTCGlobal::Instance()->api_thread()->Invoke<int32_t>(RTC_FROM_HERE, [=]() {
        int32_t res = XRTCGlobal::Instance()->GetScreenCount();
        return res;
    });
}

IVideoHandler* KRTCEngine::CreateScreenSource(const uint32_t& screen_index)
{
    return XRTCGlobal::Instance()->api_thread()->Invoke<IVideoHandler*>(RTC_FROM_HERE, [=]() {
        return new DesktopVideoSource(screen_index);
    });
}

int16_t KRTCEngine::GetMicCount() {
    return XRTCGlobal::Instance()->api_thread()->Invoke<uint32_t>(RTC_FROM_HERE, [=]() {
        if (!XRTCGlobal::Instance()->audio_device()) {
            return (int16_t)0;
        }
        return XRTCGlobal::Instance()->audio_device()->RecordingDevices();
    });
}

int32_t KRTCEngine::GetMicInfo(int index, char* mic_name, uint32_t mic_name_length,
    char* mic_guid, uint32_t mic_guid_length) 
{
    assert(mic_name_length == 128 && mic_guid_length == 128);
    return XRTCGlobal::Instance()->api_thread()->Invoke<int32_t>(RTC_FROM_HERE, [&]() {
        int32_t ret = -1;
        if (!XRTCGlobal::Instance()->audio_device()) {
            return ret;
        }
        ret = XRTCGlobal::Instance()->audio_device()->RecordingDeviceName(
            index, mic_name, mic_guid);
        return ret;
    });
}

IAudioHandler* KRTCEngine::CreateMicSource(const char* mic_id) {
   return XRTCGlobal::Instance()->api_thread()->Invoke<IAudioHandler*>(RTC_FROM_HERE, [=]() {
        return new MicImpl(mic_id);
   });
}

IMediaHandler* KRTCEngine::CreatePreview(const unsigned int& hwnd) {
   return XRTCGlobal::Instance()->api_thread()->Invoke<IMediaHandler*>(RTC_FROM_HERE, [=]() {
        return new KRTCPreview(hwnd);
   });
}

IMediaHandler* KRTCEngine::CreatePusher(const char* server_addr, const char* uid, const char* stream_name) {
   return XRTCGlobal::Instance()->api_thread()->Invoke<IMediaHandler*>(RTC_FROM_HERE, [=]() {
        return new KRTCPusher(server_addr, uid, stream_name);
   });
}

IMediaHandler* KRTCEngine::CreatePuller(const char* server_addr, const char* uid, const char* stream_name, const unsigned int& hwnd) {
    return XRTCGlobal::Instance()->api_thread()->Invoke<IMediaHandler*>(RTC_FROM_HERE, [=]() {
        return new KRTCPuller(server_addr, uid, stream_name, hwnd);
    });
}

} // namespace xrtc
