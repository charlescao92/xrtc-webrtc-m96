#include "xrtc/device/audio_track.h"

#include <rtc_base/ref_counted_object.h>

namespace xrtc {

rtc::scoped_refptr<LocalAudioSource> LocalAudioSource::Create(
    const cricket::AudioOptions* audio_options) {
    auto source = rtc::make_ref_counted<LocalAudioSource>();
    source->Initialize(audio_options);
    return source;
}

void LocalAudioSource::Initialize(const cricket::AudioOptions* audio_options) {
    if (!audio_options)
        return;

    options_ = *audio_options;
}

} // namespace xrtc