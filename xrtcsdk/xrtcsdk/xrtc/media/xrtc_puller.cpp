#include <rtc_base/thread.h>
#include <rtc_base/logging.h>
#include <rtc_base/task_utils/to_queued_task.h>
#include <rtc_base/string_encode.h>

#include "xrtc/media/xrtc_puller.h"
#include "xrtc/base/xrtc_global.h"
#include "xrtc/base/xrtc_thread.h"
#include "xrtc/media/xrtc_pull_impl.h"

namespace xrtc {

KRTCPuller::KRTCPuller(const std::string& server_addr, 
    const std::string& uid, 
    const std::string& stream_name, 
    int hwnd)
    :current_thread_(std::make_unique<KRTCThread>(rtc::Thread::Current()))
{
    pull_impl_ = new rtc::RefCountedObject<KRTCPullImpl>(server_addr, uid, stream_name, hwnd);
}

KRTCPuller::~KRTCPuller() {
}

void KRTCPuller::Start() {
    RTC_LOG(LS_INFO) << "KRTCPuller Start";

    if (pull_impl_) {
        pull_impl_->Start();
    }
}

void KRTCPuller::Stop() {
    RTC_LOG(LS_INFO) << "KRTCPuller Stop";

    if (pull_impl_) {
        pull_impl_->Stop();
        pull_impl_ = nullptr;
    }
}

void KRTCPuller::Destroy() {
    RTC_LOG(LS_INFO) << "KRTCPuller Destroy";

    delete this;
}

} // namespace xrtc
