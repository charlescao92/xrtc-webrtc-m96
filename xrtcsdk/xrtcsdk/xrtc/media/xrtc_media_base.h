#ifndef KRTCSDK_KRTC_MEDIA_KRTC_MEDIA_BASE_H_
#define KRTCSDK_KRTC_MEDIA_KRTC_MEDIA_BASE_H_

#include <string>

#include <api/peer_connection_interface.h>

#include "xrtc/xrtc.h"
#include "xrtc/tools/utils.h"

namespace xrtc {

class KRTCMediaBase {
public:
	explicit KRTCMediaBase(const CONTROL_TYPE& type, 
		const std::string& server_addr, 
		const std::string& uid,
		const std::string& stream_name,
		const int& hwnd = 0);  // 拉流相关，内部渲染则传入窗口句柄，也可以自己实现获取裸流来渲染显示
	virtual ~KRTCMediaBase();

	virtual void Start() = 0;
	virtual void Stop() = 0;

protected:
	rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> peer_connection_factory_;
	rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;

	std::string server_addr_;
	std::string stream_name_;
	std::string uid_;
	int hwnd_ = 0;
};

} // namespace xrtc

#endif // KRTCSDK_KRTC_MEDIA_KRTC_MEDIA_BASE_H_
