#include <rtc_base/thread.h>
#include <rtc_base/logging.h>
#include <rtc_base/task_utils/to_queued_task.h>
#include <rtc_base/string_encode.h>

#include "xrtc/media/xrtc_pusher.h"
#include "xrtc/base/xrtc_global.h"
#include "xrtc/base/xrtc_thread.h"
#include "xrtc/media/xrtc_push_impl.h"

namespace xrtc {

KRTCPusher::KRTCPusher(const std::string& server_addr, 
    const std::string& uid, 
    const std::string& stream_name) :
    current_thread_(std::make_unique<KRTCThread>(rtc::Thread::Current()))
{
    push_impl_ = new rtc::RefCountedObject<KRTCPushImpl>(server_addr, uid, stream_name);
}

KRTCPusher::~KRTCPusher() = default;

void KRTCPusher::Start() {
    RTC_LOG(LS_INFO) << "KRTCPusher Start";

    if (push_impl_) {
        push_impl_->Start();
    }
}

void KRTCPusher::Stop() {
    RTC_LOG(LS_INFO) << "KRTCPusher Stop";

    if (push_impl_) {
        push_impl_->Stop();
        push_impl_ = nullptr;
    }
}

void KRTCPusher::Destroy() {
    RTC_LOG(LS_INFO) << "KRTCPusher Destroy";

    delete this;
}

} // namespace xrtc
