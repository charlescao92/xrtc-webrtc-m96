#ifndef KRTCSDK_KRTC_DEVICE_CAMERA_VIDEO_SOURCE_H_
#define KRTCSDK_KRTC_DEVICE_CAMERA_VIDEO_SOURCE_H_

#include "xrtc/xrtc.h"

namespace xrtc {

class CameraVideoSource : public IVideoHandler
{
public:
	void Start() override;
	void Stop() override;
	void Destroy() override;

private:
	explicit CameraVideoSource(const char* cam_id);
	~CameraVideoSource();

	friend class KRTCEngine;
};

} // namespace xrtc

#endif // KRTCSDK_KRTC_DEVICE_CAMERA_VIDEO_SOURCE_H_
