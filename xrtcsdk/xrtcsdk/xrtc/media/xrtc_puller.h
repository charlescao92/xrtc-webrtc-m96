#ifndef KRTCSDK_KRTC_MEDIA_KRTC_PULLER_H_
#define KRTCSDK_KRTC_MEDIA_KRTC_PULLER_H_

#include "xrtc/xrtc.h"

namespace xrtc {

class KRTCThread;
class KRTCPullImpl;

class KRTCPuller : public IMediaHandler {
private:
    void Start();
    void Stop();
    void Destroy();

private:
    explicit KRTCPuller(const std::string& server_addr, const std::string& uid, const std::string& stream_name, int hwnd = 0);
    ~KRTCPuller();

    friend class KRTCEngine;

private:
    std::unique_ptr<KRTCThread> current_thread_;
    rtc::scoped_refptr<KRTCPullImpl> pull_impl_;
};

} // namespace xrtc

#endif // KRTCSDK_KRTC_MEDIA_KRTC_PULLER_H_