#include <unistd.h>

#include <rtc_base/logging.h>

#include "base/event_loop.h"
#include "server/rtc_worker.h"
#include "server/signaling_worker.h"
#include "stream/rtc_stream_manager.h"

namespace xrtc {

static void rtc_worker_recv_notify(EventLoop * /*el*/, IOWatcher */*w*/, int fd, int /*event*/, void *data) {
    int msg = -1;
    if (read(fd, &msg, sizeof(int)) != sizeof(int)) {
        RTC_LOG(LS_WARNING) << "read from pipe errror: " << strerror(errno) << ", errno: " << errno;
        return;
    }
    RtcWorker *server = (RtcWorker*)data;
    server->process_notify(msg);
}

RtcWorker::RtcWorker(int worker_id, const RtcServerOptions& options) :
    worker_id_(worker_id),
    options_(options),
    el_(new EventLoop(this)),
    rtc_stream_manager_(std::make_unique<xrtc::RtcStreamManager>(el_)) {
}

RtcWorker::~RtcWorker() {
    if (el_) {
        delete el_;
        el_ = nullptr;
    }
}

int RtcWorker::init() {
    int fds[2];
    if (pipe(fds) == -1) {
        RTC_LOG(LS_ERROR) << "create pipe error: " << strerror(errno) << ", errno: " << errno << ", worker_id:" << worker_id_;
        return -1;
    }

    notify_recv_fd_ = fds[0];
    notify_send_fd_ = fds[1];

    pipe_wather_ = el_->create_io_event(rtc_worker_recv_notify, this);
    el_->start_io_event(pipe_wather_, notify_recv_fd_, EventLoop::READ);

    return 0;
}

bool RtcWorker::start() {
    if (thread_) {
        RTC_LOG(LS_WARNING) << "rtc worker already start, worker_id:" << worker_id_;
        return false;
    }

    thread_ = std::make_unique<std::thread>([=] {
        RTC_LOG(LS_INFO) << "rtc worker event loop start, worker_id:" << worker_id_;
        el_->start();
        RTC_LOG(LS_INFO) << "rtc worker event loop stop, worker_id:" << worker_id_;
    });

    return true;
}

void RtcWorker::stop() {
    _notify(RtcWorker::QUIT);
}

int RtcWorker::_notify(int msg) {
    int writen = write(notify_send_fd_, &msg, sizeof(int));
    return writen == sizeof(int) ? 0 : -1;
}

void RtcWorker::_process_rtc_msg() {
    std::shared_ptr<RtcMsg> msg;
    if (!_pop_msg(&msg)) {
        return;
    }

    RTC_LOG(LS_INFO) << "cmdno["<< msg->cmdno 
            << "] uid[" << msg->uid 
            << "] stream_name[" << msg->stream_name 
            << "] audio[" << msg->audio
            << "] video[" << msg->video
            << "] rtc worker receive msg, worker id:" << worker_id_;

    switch (msg->cmdno) {
        case CMDNO_PUSH:
            _process_push(msg);
            break;
        case CMDNO_PULL:
            _process_pull(msg);
            break;
        case CMDNO_STOPPUSH:
            _process_stop_push(msg);
            break;
        case CMDNO_STOPPULL:
            _process_stop_pull(msg);
            break;
        default:
            RTC_LOG(LS_WARNING) << "unknown cmdno: " << msg->cmdno << ", log_id: " << msg->log_id;
            break;
    }
}

void RtcWorker::process_notify(int msg) {
    switch (msg) {
        case QUIT:
            _quit();
            break;
        case RTC_MSG:
            _process_rtc_msg();
            break;
        default:
            RTC_LOG(LS_WARNING) << "unknown msg:" << msg << ", worker_id:" << worker_id_;
            break;
    }
}

void RtcWorker::_quit() {
    if (thread_ == nullptr) {
        RTC_LOG(LS_WARNING) << "rtc server is not running, worker_id:" << worker_id_;
        return;
    }

    el_->delete_io_event(pipe_wather_);
    el_->stop();

    close(notify_recv_fd_);
    close(notify_send_fd_);

    RTC_LOG(LS_INFO) << "rtc worker quit, worker_id:" << worker_id_;
}

void RtcWorker::join() {
    if (thread_ && thread_->joinable()) {
        thread_->join();
    }
}

void RtcWorker::_push_msg(std::shared_ptr<RtcMsg> msg) {
    q_msg_.produce(msg);
}

bool RtcWorker::_pop_msg(std::shared_ptr<RtcMsg> *msg) {
    return q_msg_.consume(msg);
}

int RtcWorker::send_rtc_msg(std::shared_ptr<RtcMsg> msg) {
    _push_msg(msg);
    return _notify(RtcWorker::RTC_MSG);
}

void RtcWorker::_process_push(std::shared_ptr<RtcMsg> msg) {
    std::string offer = msg->sdp;

    std::string answer;
    int ret = rtc_stream_manager_->create_push_stream(msg, answer);
    
    RTC_LOG(LS_INFO) << "push answer: " << answer;

    if (ret != 0) {
        msg->err_no = -1;
    }
 
    msg->sdp = answer;

    SignalingWorker *worker = (SignalingWorker*)(msg->worker);
    if (worker) {
        worker->send_rtc_msg(msg);
    }
}

void RtcWorker::_process_pull(std::shared_ptr<RtcMsg> msg) {
    std::string answer;
    int ret = rtc_stream_manager_->create_pull_stream(msg, answer);
    
    RTC_LOG(LS_INFO) << "pull answer: " << answer;

    if (ret != 0) {
        msg->err_no = -1;
    }
 
    msg->sdp = answer;

    SignalingWorker *worker = (SignalingWorker*)(msg->worker);
    if (worker) {
        worker->send_rtc_msg(msg);
    }
}

void RtcWorker::_process_stop_push(std::shared_ptr<RtcMsg> msg) {
    int ret = rtc_stream_manager_->stop_push(msg->uid, msg->stream_name);
    
    RTC_LOG(LS_INFO) << "rtc worker process stop push, uid: " << msg->uid
        << ", stream_name: " << msg->stream_name
        << ", worker_id: " << worker_id_
        << ", log_id: " << msg->log_id
        << ", ret: " << ret;
}

void RtcWorker::_process_stop_pull(std::shared_ptr<RtcMsg> msg) {
    int ret = rtc_stream_manager_->stop_pull(msg->uid, msg->stream_name);
    
    RTC_LOG(LS_INFO) << "rtc worker process stop pull, uid: " << msg->uid
        << ", stream_name: " << msg->stream_name
        << ", worker_id: " << worker_id_
        << ", log_id: " << msg->log_id
        << ", ret: " << ret;
}

} // namespace xrtc
