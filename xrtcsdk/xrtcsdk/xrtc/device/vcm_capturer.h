// webrtc test/test_video_capturer.h

#ifndef KRTCSDK_KRTC_DEVICE_VCM_CAPTURER_H_
#define KRTCSDK_KRTC_DEVICE_VCM_CAPTURER_H_

#include <memory>
#include <vector>

#include <rtc_base/thread.h>
#include <api/scoped_refptr.h>
#include <modules/video_capture/video_capture.h>
#include <pc/video_track_source.h>

#include "xrtc/xrtc.h"
#include "xrtc/device/video_capturer.h"

namespace xrtc {

 class VcmCapturer : public IVideoHandler, public VideoCapturer,
                     public rtc::VideoSinkInterface<webrtc::VideoFrame> 
{
 public:
	 static std::unique_ptr<VcmCapturer> Create(const std::string& cam_id, size_t width, size_t height, 
												size_t target_fps);

     virtual ~VcmCapturer();

     void Start();
     void Stop();

     void OnFrame(const webrtc::VideoFrame& frame) override;

 private:
	 VcmCapturer(const std::string& cam_id, size_t width, size_t height, size_t target_fps);

	 void Destroy();

     std::string cam_id_;
     rtc::scoped_refptr<webrtc::VideoCaptureModule> vcm_;
     webrtc::VideoCaptureModule::DeviceInfo* device_info_;
     webrtc::VideoCaptureCapability capability_;
     rtc::Thread* signaling_thread_;
     bool has_start_ = false;

 };

 class VcmCapturerTrackSource : public webrtc::VideoTrackSource
 {
 public:
	 static rtc::scoped_refptr<VcmCapturerTrackSource> Create(const std::string& cam_id)
	 {
		 const size_t width = 640;
		 const size_t height = 480;
		 const size_t fps = 30;

		 auto vcm_capture = VcmCapturer::Create(cam_id, width, height, fps);
		 if (vcm_capture) {
			 return new rtc::RefCountedObject<VcmCapturerTrackSource>(std::move(vcm_capture));
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
	 explicit VcmCapturerTrackSource(std::unique_ptr<VcmCapturer> capture)
		 : VideoTrackSource(false)
		 , capture_(std::move(capture)) {}

 private:
	 rtc::VideoSourceInterface<webrtc::VideoFrame>* source() override {
		 return capture_.get();
	 }

	 std::unique_ptr<VcmCapturer> capture_;
 };

}  // namespace xrtc

#endif  // KRTCSDK_KRTC_DEVICE_VCM_CAPTURER_H_
