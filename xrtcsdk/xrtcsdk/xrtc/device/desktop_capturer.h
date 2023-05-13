#ifndef KRTCSDK_KRTC_DEVICE_DESKTOP_CAPTURER_H_
#define KRTCSDK_KRTC_DEVICE_DESKTOP_CAPTURER_H_

#include <thread>
#include <functional>

#include <media/base/video_common.h>
#include <media/base/video_broadcaster.h>
#include <modules/desktop_capture/desktop_capturer.h>
#include <modules/desktop_capture/desktop_capture_options.h>
#include <modules/desktop_capture/desktop_and_cursor_composer.h>
#include <media/base/video_adapter.h>
#include <media/base/video_broadcaster.h>
#include <pc/video_track_source.h>

#include "xrtc/xrtc.h"
#include "xrtc/device/video_capturer.h"

namespace xrtc {

class DesktopCapturer : public IVideoHandler, 
						public VideoCapturer,
						public rtc::VideoSinkInterface<webrtc::VideoFrame>,
						public webrtc::DesktopCapturer::Callback
{
public:
	using SourceId = webrtc::DesktopCapturer::SourceId;
	using FrameCallback = std::function<void(const webrtc::VideoFrame& frame)>;

	static bool GetScreenSourceList(webrtc::DesktopCapturer::SourceList& screen_source_list);
	static bool GetWindowSourceList(webrtc::DesktopCapturer::SourceList& window_source_list);

	static std::unique_ptr<DesktopCapturer> Create(SourceId source_id, size_t out_width = 0, size_t out_height = 0, size_t target_fps = 25);

	virtual ~DesktopCapturer();

	void SetFrameCallback(const FrameCallback& frame_callback);

	void OnFrame(const webrtc::VideoFrame& frame) override;

	void Start();
	void Stop();
	void Destroy();

private:
	DesktopCapturer();
	bool Init(webrtc::DesktopCapturer::SourceId source_id, size_t target_fps, size_t out_width = 0, size_t out_height = 0);
	
	bool CreateCapture(webrtc::DesktopCapturer::SourceId source_id);
	void CaptureThread();
	void OnCaptureResult(webrtc::DesktopCapturer::Result result, std::unique_ptr<webrtc::DesktopFrame> frame) override;

	FrameCallback frame_callback_;
	std::string title_;
	size_t out_width_ = 0;
	size_t out_height_ = 0;
	size_t target_fps_ = 25;
	webrtc::DesktopCaptureOptions capture_options_;
	webrtc::DesktopCapturer::SourceId source_id_;
	std::unique_ptr<webrtc::DesktopCapturer> desktop_capturer_;

	bool is_capturing_ = false;
	bool is_capture_cursor_ = true;
	std::unique_ptr<std::thread> capture_thread_;

};

class DesktopCapturerTrackSource : public webrtc::VideoTrackSource
{
public:
	static rtc::scoped_refptr<DesktopCapturerTrackSource> Create(const uint32_t& screen_index, size_t target_fps = 30)
	{
		webrtc::DesktopCapturer::SourceList source_list;
		DesktopCapturer::GetScreenSourceList(source_list);

		auto screen_capture = DesktopCapturer::Create(source_list[screen_index].id, target_fps);
		if (screen_capture) {
			return new rtc::RefCountedObject<DesktopCapturerTrackSource>(std::move(screen_capture));
		}
		return nullptr;
	}

	void Start() {
		capture_->Start();
	}

	void Stop() {
		capture_->Stop();
	}

protected:
	explicit DesktopCapturerTrackSource(std::unique_ptr<DesktopCapturer> capture)
		: VideoTrackSource(false)
		, capture_(std::move(capture)) {}

private:
	rtc::VideoSourceInterface<webrtc::VideoFrame>* source() override
	{
		return capture_.get();
	}

	std::unique_ptr<DesktopCapturer> capture_;
};

} // namespace xrtc

#endif // KRTCSDK_KRTC_DEVICE_DESKTOP_CAPTURER_H_