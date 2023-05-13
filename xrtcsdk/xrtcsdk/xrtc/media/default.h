#ifndef KRTCSDK_KRTC_MEDIA_BASE_DEFAULT_H_
#define KRTCSDK_KRTC_MEDIA_BASE_DEFAULT_H_

#include <rtc_base/ref_counted_object.h>
#include <api/jsep.h>

const char kAudioLabel[] = "audio_label";
const char kVideoLabel[] = "video_label";
const char kStreamId[] = "stream_id";

class DummySetSessionDescriptionObserver
    : public webrtc::SetSessionDescriptionObserver {
public:
    static DummySetSessionDescriptionObserver* Create() {
        return new rtc::RefCountedObject<DummySetSessionDescriptionObserver>();
    }
    virtual void OnSuccess() { RTC_LOG(INFO) << __FUNCTION__; }
    virtual void OnFailure(webrtc::RTCError error) {
        RTC_LOG(INFO) << __FUNCTION__ << " " << ToString(error.type()) << ": "
            << error.message();
    }
};

#endif // KRTCSDK_KRTC_MEDIA_BASE_DEFAULT_H_