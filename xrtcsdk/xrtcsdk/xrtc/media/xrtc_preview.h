#ifndef KRTCSDK_KRTC_MEDIA_KRTC_PREVIEW_H_
#define KRTCSDK_KRTC_MEDIA_KRTC_PREVIEW_H_

#include "xrtc/xrtc.h"

#include <api/media_stream_interface.h>

namespace xrtc {

class KRTCThread;
class VideoRenderer;

class KRTCPreview : public IMediaHandler,
					public rtc::VideoSinkInterface<webrtc::VideoFrame> {
public:
	explicit KRTCPreview(const int& hwnd = 0);
	~KRTCPreview();

public:
	void Start();
	void Stop();
	void Destroy();

	void OnFrame(const webrtc::VideoFrame& frame) override;

private:
	std::unique_ptr<KRTCThread> current_thread_;
	std::unique_ptr<VideoRenderer> local_renderer_;
	int hwnd_;
};

}
#endif // KRTCSDK_KRTC_MEDIA_KRTC_PREVIEW_H_