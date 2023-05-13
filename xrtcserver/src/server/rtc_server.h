/**
 * @file rtc_server.hpp
 * @author charles
 * @brief 
*/

#ifndef __RTC_SERVER_H_
#define __RTC_SERVER_H_

#include <memory>
#include <thread>
#include <queue>
#include <mutex>
#include <vector>

#include <rtc_base/rtc_certificate.h>

#include "xrtcserver_def.h"
#include "server/settings.h"

namespace xrtc {

class EventLoop;
class IOWatcher;
class RtcWorker;

class RtcServer {
public:
    enum {
        QUIT = 0,
        RTC_MSG = 1,
    };

    RtcServer();
    ~RtcServer();

public:
    int init(const RtcServerOptions& options);
    bool start();
    void stop();
    int notify(int msg);
    void process_notify(int msg);
    void join();
    int send_rtc_msg(std::shared_ptr<RtcMsg> msg);
    void push_msg(std::shared_ptr<RtcMsg> msg);
    std::shared_ptr<RtcMsg> pop_msg();
    void process_rtc_msg();

private:
    void _quit();
    int _create_worker(int worker_id);
    std::shared_ptr<RtcWorker> _get_worker(const std::string &stream_name);
    int _generate_and_check_certificate();

private:
    RtcServerOptions options_;
    std::unique_ptr<EventLoop> el_;
    std::unique_ptr<std::thread> thread_;

    IOWatcher *pipe_wather_ = nullptr;
    int notify_recv_fd_ = -1;
    int notify_send_fd_ = -1;

    std::queue<std::shared_ptr<RtcMsg>> q_msg_;
    std::mutex q_msg_mtx_;

    std::vector<std::shared_ptr<RtcWorker>> workers_;
    rtc::scoped_refptr<rtc::RTCCertificate> certificate_;

};

} // namespace xrtc

#endif // __RTC_SERVER_H_
