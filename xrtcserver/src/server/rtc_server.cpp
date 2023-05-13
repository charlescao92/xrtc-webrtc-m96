#include <unistd.h>

#include <rtc_base/logging.h>
#include <rtc_base/crc32.h>
#include <rtc_base/rtc_certificate_generator.h>

#include <yaml-cpp/yaml.h>

#include "base/event_loop.h"
#include "server/rtc_server.h"
#include "server/rtc_worker.h"

namespace xrtc {

const uint64_t k_year_in_ms = 365 * 24 * 3600 * 1000L;

RtcServer::RtcServer() : 
    el_(std::make_unique<xrtc::EventLoop>(this)) {
}

RtcServer::~RtcServer() {
    workers_.clear();
    workers_.shrink_to_fit();
}

static void rtc_server_recv_notify(EventLoop * /*el*/, IOWatcher */*w*/, int fd, int /*event*/, void *data) {
    int msg = -1;
    if (read(fd, &msg, sizeof(int)) != sizeof(int)) {
        RTC_LOG(LS_WARNING) << "read from pipe errror: " << strerror(errno) << ", errno: " << errno;
        return;
    }

    RtcServer *server = (RtcServer*)data;
    server->process_notify(msg);
}

int RtcServer::_generate_and_check_certificate() {
    if (!certificate_ || certificate_->HasExpired(time(NULL) * 1000)) {
        rtc::KeyParams key_params;
        RTC_LOG(LS_INFO) << "dtls enabled, key type: " << key_params.type();
        certificate_ = rtc::RTCCertificateGenerator::GenerateCertificate(key_params, k_year_in_ms);
        if (certificate_) {
            rtc::RTCCertificatePEM pem = certificate_->ToPEM();
            RTC_LOG(LS_INFO) << "rtc certificate: \n" << pem.certificate();
        }
    }
    
    if (!certificate_) {
        RTC_LOG(LS_WARNING) << "get certificate error";
        return -1;
    }

    return 0;
}

int RtcServer::init(const RtcServerOptions& options) {  
    options_ = options;
    
    // 生成证书
    if (_generate_and_check_certificate() != 0) {
        return -1;
    }

    int fds[2];
    if (pipe(fds) == -1) {
        RTC_LOG(LS_ERROR) << "create pipe error: " << strerror(errno) << ", errno: " << errno;
        return -1;
    }

    notify_recv_fd_ = fds[0];
    notify_send_fd_ = fds[1];

    // 将recv_fd添加到事件循环，进行管理
    pipe_wather_ = el_->create_io_event(rtc_server_recv_notify, this);
    el_->start_io_event(pipe_wather_, notify_recv_fd_, EventLoop::READ);

    // 创建worker
    for (int i = 0; i < options_.worker_num; ++i) {
        if (_create_worker(i) != 0) {
            return -1;
        }
    }

    return 0;
}

bool RtcServer::start() {
    if (thread_ != nullptr) {
        RTC_LOG(LS_WARNING) << "rtc server already run";
        return false;
    }

    thread_ = std::make_unique<std::thread>([=] {
        RTC_LOG(LS_INFO) << "rtc server event loop run";
        el_->start();
        RTC_LOG(LS_INFO) << "rtc server event loop stop";
    });

    return true;
}

void RtcServer::stop() {
    notify(RtcServer::QUIT);
}

int RtcServer::notify(int msg) {
    int writen = write(notify_send_fd_, &msg, sizeof(int));
    return writen == sizeof(int) ? 0 : -1;
}

void RtcServer::process_notify(int msg) {
    switch (msg) {
        case QUIT:
            _quit();
            break;
        case RTC_MSG:
            process_rtc_msg();
            break;
        default:
            RTC_LOG(LS_WARNING) << "unknown msg:" << msg;
            break;
    }
}

void RtcServer::_quit() {
    if (thread_ == nullptr) {
        RTC_LOG(LS_WARNING) << "rtc server is not running";
        return;
    }

    el_->delete_io_event(pipe_wather_);
    el_->stop();

    close(notify_recv_fd_);
    close(notify_send_fd_);

    for (auto worker : workers_) {
        if (worker) {
            worker->stop();
            worker->join();
        }
    }

    RTC_LOG(LS_INFO) << "rtc server quit";
}

void RtcServer::join() {
    if (thread_ && thread_->joinable()) {
        thread_->join();
    }
}

int RtcServer::send_rtc_msg(std::shared_ptr<RtcMsg> rtcMsg) {
    push_msg(rtcMsg);
    return notify(RtcServer::RTC_MSG);
}

void RtcServer::push_msg(std::shared_ptr<RtcMsg> msg) {
    std::unique_lock<std::mutex> lock(q_msg_mtx_);
    q_msg_.push(msg);
}

std::shared_ptr<RtcMsg> RtcServer::pop_msg() {
    std::unique_lock<std::mutex> lock(q_msg_mtx_);

    if (q_msg_.empty()) {
        return nullptr;
    }

    std::shared_ptr<RtcMsg> msg = q_msg_.front();
    q_msg_.pop();

    return msg;
}

std::shared_ptr<RtcWorker> RtcServer::_get_worker(const std::string &stream_name) {
    if (workers_.size() == 0 || workers_.size() != (size_t)options_.worker_num) {
        return nullptr;
    }

    uint32_t num = rtc::ComputeCrc32(stream_name);
    size_t index = num % options_.worker_num;
    return workers_[index];
}

void RtcServer::process_rtc_msg() {
    std::shared_ptr<RtcMsg> msg = pop_msg();
    if (!msg) {
        return;
    }

    if (_generate_and_check_certificate() != 0) {
        return;
    }
    
    msg->certificate = certificate_.get();

    std::shared_ptr<RtcWorker> worker = _get_worker(msg->stream_name);
    if (worker) {
        worker->send_rtc_msg(msg);
    }
}

int RtcServer::_create_worker(int worker_id) {
    RTC_LOG(LS_INFO) << "rtc server create worker, worker_id:" << worker_id;

    auto worker = std::make_shared<RtcWorker>(worker_id, options_);
    if (worker->init() != 0) {
        return -1;
    }

    if (!worker->start()) {
        return -1;
    }

    workers_.push_back(worker);

    return 0;    
}

}
