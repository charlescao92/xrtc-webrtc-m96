/**
 * @file signaling_worker.h
 * @author charles
 * @brief 
*/

#ifndef __RTC_WORKER_H_
#define __RTC_WORKER_H_

#include <memory>
#include <thread>

#include "server/rtc_server.h"
#include "xrtcserver_def.h"
#include "base/lock_free_queue.h"
#include "server/settings.h"

namespace xrtc {

class EventLoop;
class IOWatcher;
class RtcStreamManager;

class RtcWorker {
public:
    enum {
        QUIT = 0,
        RTC_MSG = 1,
    };

    explicit RtcWorker(int worker_id, const RtcServerOptions& options);
    ~RtcWorker();

public:
    int init();
    bool start();
    void stop();
    void process_notify(int msg);
    void join();
    int send_rtc_msg(std::shared_ptr<RtcMsg> msg);

private:
    void _quit();
    int _notify(int msg);
    void _push_msg(std::shared_ptr<RtcMsg> msg);
    bool _pop_msg(std::shared_ptr<RtcMsg> *msg);
    void _process_rtc_msg();
    void _process_push(std::shared_ptr<RtcMsg> msg);
    void _process_pull(std::shared_ptr<RtcMsg> msg);
    void _process_stop_push(std::shared_ptr<RtcMsg> msg);
    void _process_stop_pull(std::shared_ptr<RtcMsg> msg);

private:
    int worker_id_;
    RtcServerOptions options_;
    EventLoop *el_ = nullptr;

    IOWatcher *pipe_wather_ = nullptr;
    int notify_recv_fd_ = -1;
    int notify_send_fd_ = -1;

    std::unique_ptr<std::thread> thread_;
    LockFreeQueue<std::shared_ptr<RtcMsg>> q_msg_;
    std::unique_ptr<RtcStreamManager> rtc_stream_manager_;
};

} // end namespace xrtc

#endif // __RTC_WORKER_H_
