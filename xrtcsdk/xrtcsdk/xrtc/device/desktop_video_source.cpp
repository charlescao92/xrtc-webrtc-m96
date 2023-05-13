#include "xrtc/device/desktop_video_source.h"
#include "xrtc/base/xrtc_global.h"

namespace xrtc {

DesktopVideoSource::DesktopVideoSource(uint16_t screen_index, uint16_t target_fps) {
	XRTCGlobal::Instance()->CreateDesktopCapturerSource(screen_index, target_fps);
}

DesktopVideoSource::~DesktopVideoSource() {}

void DesktopVideoSource::Start() {
	XRTCGlobal::Instance()->StartDesktopCapturerSource();
}

void DesktopVideoSource::Stop() {
	XRTCGlobal::Instance()->StopDesktopCapturerSource();
}

void DesktopVideoSource::Destroy() {}

} // namespace xrtc
