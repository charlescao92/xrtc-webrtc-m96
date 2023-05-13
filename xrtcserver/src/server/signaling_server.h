/**
 * @file signaling_server.hpp
 * @author charles
 * @brief 
*/

#ifndef __SIGNALING_SERVER_H_
#define __SIGNALING_SERVER_H_

#include <string>
#include <memory>
#include <thread>
#include <vector>

#include "server/settings.h"

namespace xrtc {

class EventLoop;
class IOWatcher;
class SignalingWorker;

class SignalingServer {
public:
    enum {
        QUIT = 0,
    };

    SignalingServer();
    ~SignalingServer();

public:
    int init(const SignalingServerOptions& options);
    bool start();
    void stop();
    int notify(int msg);
    void process_notify(int msg);
    void join();

    void dispatch_new_conn(int fd);

private:
    void _quit();
    int _create_worker(int worker_id);
    
private:
    SignalingServerOptions options_;
    std::unique_ptr<EventLoop> el_;
    IOWatcher *io_watcher_ = nullptr;

    IOWatcher *pipe_wather_ = nullptr;
    int notify_recv_fd_ = -1;
    int notify_send_fd_ = -1;

    std::unique_ptr<std::thread> thread_;

    int listen_fd_ = -1;
    std::vector<std::shared_ptr<SignalingWorker>> workers_;
    size_t next_worker_index_ = 0;
};

} // namespace xrtc

#endif // __SIGNALING_SERVER_H_
