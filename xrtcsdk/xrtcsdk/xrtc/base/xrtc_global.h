#ifndef KRTCSDK_KRTC_BASE_KRTC_GLOBAL_H_
#define KRTCSDK_KRTC_BASE_KRTC_GLOBAL_H_

#include <memory>

#include <rtc_base/thread.h>
#include <modules/video_capture/video_capture.h>
#include <modules/audio_device/include/audio_device.h>
#include <api/task_queue/default_task_queue_factory.h>
#include <api/media_stream_interface.h>
#include <api/peer_connection_interface.h>

#include "xrtc/device/vcm_capturer.h"
#include "xrtc/device/desktop_capturer.h"

namespace xrtc {

	class KRTCEngineObserver;
	class HttpManager;

    enum class CAPTURE_TYPE {
		CAMERA,	// 摄像头采集
		SCREEN  // 桌面采集
	};

	// 全局管理类，单例模式
	class XRTCGlobal {
	public:
		static XRTCGlobal* Instance();

	private:
		XRTCGlobal();
		~XRTCGlobal();

	public:
		rtc::Thread* api_thread() { return signaling_thread_.get(); }
		rtc::Thread* worker_thread() { return worker_thread_.get(); }
		rtc::Thread* network_thread() { return network_thread_.get(); }
		
		webrtc::VideoCaptureModule::DeviceInfo* video_device_info() {
			return video_device_info_.get();
		}

		void SetPreview(bool preview) { is_preview_ = preview; }
		bool is_preview() const { return is_preview_;  }

		size_t GetScreenCount() const { return screen_source_list_.size(); }

		KRTCEngineObserver* engine_observer() { return engine_observer_; }
		void RegisterEngineObserver(KRTCEngineObserver* observer) {
			engine_observer_ = observer;
		}

		HttpManager* http_manager() { return http_manager_; }
	
		webrtc::AudioDeviceModule* audio_device() { return audio_device_.get(); }

		webrtc::PeerConnectionFactoryInterface* push_peer_connection_factory();

		webrtc::TaskQueueFactory* task_queue_factory() { return task_queue_factory_.get(); }

		webrtc::VideoTrackSource* current_video_source();

		void SetCurrentCaptureType(const CAPTURE_TYPE& type) {
			current_capture_type_ = type;
		}

		CAPTURE_TYPE current_capture_type() const {
			return current_capture_type_;
		}

		void CreateVcmCapturerSource(const char* cam_id);
		void StartVcmCapturerSource();
		void StopVcmCapturerSource();

		void CreateDesktopCapturerSource(uint16_t screen_index, uint16_t target_fps);
		void StartDesktopCapturerSource();
		void StopDesktopCapturerSource();

	private:
		std::unique_ptr<rtc::Thread> signaling_thread_;
		std::unique_ptr<rtc::Thread> worker_thread_;
		std::unique_ptr<rtc::Thread> network_thread_;
		std::unique_ptr<webrtc::VideoCaptureModule::DeviceInfo> video_device_info_;
		std::unique_ptr<webrtc::TaskQueueFactory> task_queue_factory_;
		rtc::scoped_refptr<webrtc::AudioDeviceModule> audio_device_;
		KRTCEngineObserver* engine_observer_ = nullptr;
		rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> push_peer_connection_factory_;
		rtc::scoped_refptr<VcmCapturerTrackSource> camera_capturer_source_;
		rtc::scoped_refptr<DesktopCapturerTrackSource> desktop_capturer_source_;
		webrtc::DesktopCapturer::SourceList screen_source_list_;
		CAPTURE_TYPE current_capture_type_ = CAPTURE_TYPE::CAMERA;
		HttpManager* http_manager_ = nullptr;
		bool is_preview_ = false;
	};

} // namespace xrtc

#endif // KRTCSDK_KRTC_BASE_KRTC_GLOBAL_H_
