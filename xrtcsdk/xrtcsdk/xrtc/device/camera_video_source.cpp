#include "xrtc/device/camera_video_source.h"

#include "xrtc/base/xrtc_global.h"

namespace xrtc {

CameraVideoSource::CameraVideoSource(const char* cam_id)
{
    XRTCGlobal::Instance()->CreateVcmCapturerSource(cam_id);
}

CameraVideoSource::~CameraVideoSource() {
    Destroy();
}

void CameraVideoSource::Start() {
    XRTCGlobal::Instance()->StartVcmCapturerSource();      
}

void CameraVideoSource::Stop() {
    XRTCGlobal::Instance()->StopVcmCapturerSource();
}

void CameraVideoSource::Destroy() {
}

} // namespace xrtc
