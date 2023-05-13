#include "xrtc/base/xrtc_thread.h"

namespace xrtc {

KRTCThread::KRTCThread(rtc::Thread* current_thread) :
	current_thread_(current_thread) {}

}