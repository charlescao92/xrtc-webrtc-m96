#include <unistd.h>

#include <rtc_base/logging.h>
#include <yaml-cpp/yaml.h>

#include "base/socket.h"
#include "base/event_loop.h"
#include "server/signaling_server.h"
#include "server/signaling_worker.h"

namespace xrtc {

static void accept_new_conn(EventLoop * /*el*/, IOWatcher * /*w*/, int fd, int /*event*/, void *data) {
    int cfd = -1;
    char cip[128] = {0};
    int cport = 0;

    cfd = tcp_accept(fd, cip, &cport);
    if (-1 == cfd) {
        return;
    }

    RTC_LOG(LS_INFO) << "accept new conn, fd: " << cfd << ", ip: " << cip << ", port: " << cport;

    SignalingServer *server = (SignalingServer*)data;
    server->dispatch_new_conn(cfd);
}

static void signaling_server_recv_notify(EventLoop * /*el*/, IOWatcher */*w*/, int fd, int /*event*/, void *data) {
    int msg = -1;
    if (read(fd, &msg, sizeof(int)) != sizeof(int)) {
        RTC_LOG(LS_WARNING) << "read from pipe errror: " << strerror(errno) << ", errno: " << errno;
        return;
    }
    SignalingServer *server = (SignalingServer*)data;
    server->process_notify(msg);
}

SignalingServer::SignalingServer() : 
    el_(std::make_unique<xrtc::EventLoop>(this)) {
}

SignalingServer::~SignalingServer() {
    workers_.clear();
    workers_.shrink_to_fit();
}

int SignalingServer::init(const SignalingServerOptions& options) {
    options_ = options;

    // 创建管道，用于线程间通信
    int fds[2];
    if (pipe(fds) == -1) {
        RTC_LOG(LS_ERROR) << "create pipe error: " << strerror(errno) << ", errno: " << errno;
        return -1;
    }

    notify_recv_fd_ = fds[0];
    notify_send_fd_ = fds[1];

    // 将recv_fd添加到事件循环，进行管理
    pipe_wather_ = el_->create_io_event(signaling_server_recv_notify, this);
    el_->start_io_event(pipe_wather_, notify_recv_fd_, EventLoop::READ);

    // 创建tcp server
    listen_fd_ = create_tcp_server(options_.host_ip.c_str(), options_.port);
    if (listen_fd_ == -1) {
        RTC_LOG(LS_ERROR) << "create tcp server failed";
        return -1;
    }

    // 创建并启动io事件监听
    io_watcher_ = el_->create_io_event(accept_new_conn, this);
    el_->start_io_event(io_watcher_, listen_fd_, xrtc::EventLoop::READ);

    // 创建worker
    for (int i = 0; i < options_.worker_num; ++i) {
        if (_create_worker(i) != 0) {
            return -1;
        }
    }

    return 0;
}

bool SignalingServer::start() {
    if (thread_ != nullptr) {
        RTC_LOG(LS_WARNING) << "signaling server already run";
        return false;
    }

    thread_ = std::make_unique<std::thread>([=] {
        RTC_LOG(LS_INFO) << "signaling server event loop run";
        el_->start();
        RTC_LOG(LS_INFO) << "signaling server event loop stop";
    });

    return true;
}

void SignalingServer::stop() {
    notify(SignalingServer::QUIT);
}

int SignalingServer::notify(int msg) {
    int writen = write(notify_send_fd_, &msg, sizeof(int));
    return writen == sizeof(int) ? 0 : -1;
}

void SignalingServer::process_notify(int msg) {
    switch (msg) {
        case QUIT:
            _quit();
            break;
        default:
            RTC_LOG(LS_WARNING) << "unknown msg:" << msg;
            break;
    }
}

void SignalingServer::_quit() {
    if (thread_ == nullptr) {
        RTC_LOG(LS_WARNING) << "signaling server is not running";
        return;
    }

    el_->delete_io_event(pipe_wather_);
    el_->delete_io_event(io_watcher_);
    el_->stop();

    close(notify_recv_fd_);
    close(notify_send_fd_);
    close(listen_fd_);

    for (auto worker : workers_) {
        if (worker) {
            worker->stop();
            worker->join();
        }
    }

    RTC_LOG(LS_INFO) << "signaling server quit";
}

void SignalingServer::join() {
    if (thread_ && thread_->joinable()) {
        thread_->join();
    }
}

int SignalingServer::_create_worker(int worker_id) {
    RTC_LOG(LS_INFO) << "signaling server create worker, worker_id:" << worker_id;

    auto worker = std::make_shared<SignalingWorker>(worker_id, options_);
    if (worker->init() != 0) {
        return -1;
    }

    if (!worker->start()) {
        return -1;
    }

    workers_.push_back(worker);

    return 0;
}

void SignalingServer::dispatch_new_conn(int fd) {
    size_t index = next_worker_index_;
    next_worker_index_++;
    if (next_worker_index_ >= workers_.size()) {
        next_worker_index_ = 0;
    }

    auto worker = workers_.at(index);
    worker->notify_new_conn(fd);
}

} // namespace xrtc
