#ifndef KRTCSDK_KRTC_DEVICE_MIC_IMPL_H_
#define KRTCSDK_KRTC_DEVICE_MIC_IMPL_H_

#include <mutex>

#include <rtc_base/thread.h>
#include <modules/audio_device/include/audio_device.h>

#include "xrtc/xrtc.h"

namespace xrtc {

class MicImpl : public IAudioHandler, webrtc::AudioTransport
{
public:
    void Start() override;
    void Stop() override;
    void Destroy() override;
 
private:
    explicit MicImpl(const char* mic_id);
    ~MicImpl() override;

    int32_t RecordedDataIsAvailable(const void* audioSamples,
        const size_t nSamples,
        const size_t nBytesPerSample,
        const size_t nChannels,
        const uint32_t samplesPerSec,
        const uint32_t totalDelayMS,
        const int32_t clockDrift,
        const uint32_t currentMicLevel,
        const bool keyPressed,
        uint32_t& newMicLevel);  // NOLINT

    // Implementation has to setup safe values for all specified out parameters.
    // 播放相关的，不用实现
    int32_t NeedMorePlayData(const size_t nSamples,
        const size_t nBytesPerSample,
        const size_t nChannels,
        const uint32_t samplesPerSec,
        void* audioSamples,
        size_t& nSamplesOut,  // NOLINT
        int64_t* elapsed_time_ms,
        int64_t* ntp_time_ms) override {
        return -1;
    } // NOLINT

    void PullRenderData(int bits_per_sample,
        int sample_rate,
        size_t number_of_channels,
        size_t number_of_frames,
        void* audio_data,
        int64_t* elapsed_time_ms,
        int64_t* ntp_time_ms) {}

private:
    std::string mic_id_;
    rtc::Thread* signaling_thread_;
    bool has_start_ = false;
    std::mutex mtx_;
    uint32_t timestamp_ = 0;

    friend class KRTCEngine;
};

} // namespace xrtc

#endif // KRTCSDK_KRTC_DEVICE_MIC_IMPL_H_
