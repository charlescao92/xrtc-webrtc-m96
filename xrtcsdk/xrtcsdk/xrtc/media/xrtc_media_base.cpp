#include "xrtc/media/xrtc_media_base.h"

#include <rtc_base/string_encode.h>

namespace xrtc {

KRTCMediaBase::KRTCMediaBase(const CONTROL_TYPE& type, 
	const std::string& server_addr, 
	const std::string& uid,
	const std::string& stream_name,
	const int& hwnd) :
	server_addr_(server_addr),
	uid_(uid),
	stream_name_(stream_name),
	hwnd_(hwnd)
{
}

KRTCMediaBase::~KRTCMediaBase() {}

} // namespace xrtc
