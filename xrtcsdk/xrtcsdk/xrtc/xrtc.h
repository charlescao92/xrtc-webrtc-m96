#ifndef KRTCSDK_KRTC_KRTC_H_
#define KRTCSDK_KRTC_KRTC_H_

#ifdef KRTC_STATIC
#define KRTC_API
#else
#ifdef KRTC_API_EXPORT
#if defined(_MSC_VER)
#define KRTC_API __declspec(dllexport)
#else
#define KRTC_API
#endif
#else
#if defined(_MSC_VER)
#define KRTC_API __declspec(dllimport)
#else
#define KRTC_API
#endif
#endif
#endif

#include <memory>

namespace xrtc {
 
class MediaFrame;
class KRTCRender;
class KRTCPreview;
class KRTCPusher;
class KRTCPuller;

enum class KRTCError {
    kNoErr = 0,
    kVideoCreateCaptureErr,
    kVideoNoCapabilitiesErr,
    kVideoNoBestCapabilitiesErr,
    kVideoStartCaptureErr,
    kPreviewNoVideoSourceErr,
    kPreviewNoRenderHwndErr,
    kPreviewCreateRenderDeviceErr,
    kInvalidUrlErr,
    kAddTrackErr,
    kCreateOfferErr,
    kSendOfferErr,
    kParseAnswerErr,
    kAnswerResponseErr,
    kNoAudioDeviceErr,
    kAudioNotFoundErr,
    kAudioSetRecordingDeviceErr,
    kAudioInitRecordingErr,
    kAudioStartRecordingErr
};

class IMediaHandler {
public:
    virtual ~IMediaHandler() {}

    virtual void Start() = 0;
    virtual void Stop() = 0;
    virtual void Destroy() = 0;
};

class IAudioHandler : public IMediaHandler {
};

class IVideoHandler : public IMediaHandler {
};

class KRTC_API KRTCEngineObserver {
public:
    virtual void OnVideoSourceSuccess() {}
    virtual void OnVideoSourceFailed(KRTCError) {}
    virtual void OnAudioSourceSuccess() {}
    virtual void OnAudioSourceFailed(KRTCError) {}
    virtual void OnPreviewSuccess() {}
    virtual void OnPreviewFailed(KRTCError) {}
    virtual void OnPushSuccess() {}
    virtual void OnPushFailed(KRTCError) {}
    virtual void OnPullSuccess() {}
    virtual void OnPullFailed(KRTCError) {}
    virtual void OnNetworkInfo(uint64_t rtt_ms, uint64_t packets_lost, double fraction_lost) {}
    virtual void OnVideoCaptureFps(uint32_t fps) {}
    virtual void OnEncodedVideoFrame(std::shared_ptr<MediaFrame> video_frame) {}
    virtual void OnPureAudioFrame(std::shared_ptr<MediaFrame> audio_frame) {}
    virtual void OnMixedAudioFrame(std::shared_ptr<MediaFrame> audio_frame) {}
    virtual void OnProcessingFilterAudioFrame(std::shared_ptr<MediaFrame> audio_frame) {}
    virtual void OnEncodedAudioFrame(std::shared_ptr<MediaFrame> audio_frame) {}
    virtual void OnCapturePureVideoFrame(std::shared_ptr<xrtc::MediaFrame> video_frame) {}
    virtual void OnPullVideoFrame(std::shared_ptr<xrtc::MediaFrame> video_frame) {}
};

enum class KRTC_API CONTROL_TYPE {
    PUSH,
    PULL,
};

class KRTC_API KRTCEngine {
public:
    static void Init(KRTCEngineObserver* observer);
    static const char* GetErrString(const KRTCError& err);

    static uint32_t GetCameraCount();
    static int32_t GetCameraInfo(int index, char *device_name, uint32_t device_name_length,
        char* device_id, uint32_t device_id_length);
    static IVideoHandler* CreateCameraSource(const char* cam_id);

    static uint32_t GetScreenCount();
    static IVideoHandler* CreateScreenSource(const uint32_t& screen_index = 0);
   
    static int16_t GetMicCount();
    static int32_t GetMicInfo(int index, char* mic_name, uint32_t mic_name_length,
        char* mic_guid, uint32_t mic_guid_length);
    static IAudioHandler* CreateMicSource(const char* mic_id);

    static IMediaHandler* CreatePreview(const unsigned int& hwnd = 0);
    static IMediaHandler* CreatePusher(const char* server_addr,
        const char* uid,
        const char* stream_name);
    static IMediaHandler* CreatePuller(const char* server_addr, 
        const char* uid,
        const char* stream_name,
        const unsigned int& hwnd = 0);
};

} // namespace xrtc

#endif // KRTCSDK_KRTC_KRTC_H_
