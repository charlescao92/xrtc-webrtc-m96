#ifndef KRTCSDK_KRTC_MEDIA_KRTC_PUSHER_H_
#define KRTCSDK_KRTC_MEDIA_KRTC_PUSHER_H_

#include "xrtc/xrtc.h"

namespace xrtc {

class KRTCThread;
class KRTCPushImpl;

class KRTCPusher : public IMediaHandler {
private:
    void Start();
    void Stop();
    void Destroy();

private:
    explicit KRTCPusher(const std::string& server_addr, 
        const std::string& uid,
        const std::string& stream_name);
    ~KRTCPusher();

private:
    std::unique_ptr<KRTCThread> current_thread_;
    rtc::scoped_refptr<KRTCPushImpl> push_impl_;

    friend class KRTCEngine;
};

} // namespace xrtc

#endif // KRTCSDK_KRTC_MEDIA_BASE_KRTC_PUSHER_H_
