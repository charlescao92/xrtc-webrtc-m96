#include <unistd.h>

#include <rtc_base/logging.h>
#include <rtc_base/zmalloc.h>

#include "xrtcserver_def.h"
#include "base/event_loop.h"
#include "base/socket.h"
#include "base/xhead.h"
#include "server/signaling_worker.h"
#include "server/tcp_connection.h"
#include "server/rtc_server.h"

extern xrtc::RtcServer *g_rtc_server;

namespace xrtc {

static void signaling_worker_recv_notify(EventLoop * /*el*/, IOWatcher */*w*/, int fd, int /*event*/, void *data) {
    int msg = -1;
    if (read(fd, &msg, sizeof(int)) != sizeof(int)) {
        RTC_LOG(LS_WARNING) << "read from pipe errror: " << strerror(errno) << ", errno: " << errno;
        return;
    }
    SignalingWorker *server = (SignalingWorker*)data;
    server->process_notify(msg);
}

SignalingWorker::SignalingWorker(int worker_id, const SignalingServerOptions& options) :
    worker_id_(worker_id),
    options_(options),
    el_(std::make_unique<xrtc::EventLoop>(this)) {
}

SignalingWorker::~SignalingWorker() {
    for (auto conn : conns_) {
        if (conn) {
            _close_connection(conn);
            conn = nullptr;
        }
    }

    conns_.clear();
    conns_.shrink_to_fit();
}

int SignalingWorker::init() {
    int fds[2];
    if (pipe(fds) == -1) {
        RTC_LOG(LS_ERROR) << "create pipe error: " << strerror(errno) << ", errno: " << errno << ", worker_id:" << worker_id_;
        return -1;
    }

    notify_recv_fd_ = fds[0];
    notify_send_fd_ = fds[1];

    pipe_wather_ = el_->create_io_event(signaling_worker_recv_notify, this);
    el_->start_io_event(pipe_wather_, notify_recv_fd_, EventLoop::READ);

    return 0;
}

bool SignalingWorker::start() {
    if (thread_) {
        RTC_LOG(LS_WARNING) << "signaling worker already start, worker_id:" << worker_id_;
        return false;
    }

    thread_ = std::make_unique<std::thread>([=] {
        RTC_LOG(LS_INFO) << "signaling worker event loop start, worker_id:" << worker_id_;
        el_->start();
        RTC_LOG(LS_INFO) << "signaling worker event loop stop, worker_id:" << worker_id_;
    });

    return true;
}

void SignalingWorker::stop() {
    _notify(SignalingWorker::QUIT);
}

int SignalingWorker::_notify(int msg) {
    int writen = write(notify_send_fd_, &msg, sizeof(int));
    return writen == sizeof(int) ? 0 : -1;
}

void SignalingWorker::process_notify(int msg) {
    switch (msg) {
        case QUIT:
            _quit();
            break;
        case NEW_CONN:
            int fd;
            if (q_conn_.consume(&fd)) {
                _handle_new_conn(fd);
            }
            break;
        case RTC_MSG:
            _process_rtc_msg();
            break;
        default:
            RTC_LOG(LS_WARNING) << "unknown msg:" << msg << ", worker_id:" << worker_id_;
            break;
    }
}

void SignalingWorker::_quit() {
    if (thread_ == nullptr) {
        RTC_LOG(LS_WARNING) << "signaling server is not running, worker_id:" << worker_id_;
        return;
    }

    el_->delete_io_event(pipe_wather_);
    el_->stop();

    close(notify_recv_fd_);
    close(notify_send_fd_);

    RTC_LOG(LS_INFO) << "signaling worker quit, worker_id:" << worker_id_;
}

void SignalingWorker::join() {
    if (thread_ && thread_->joinable()) {
        thread_->join();
    }
}

void SignalingWorker::notify_new_conn(int fd) {
    RTC_LOG(LS_WARNING) << "notify_new_conn, worker_id:" << worker_id_;
    q_conn_.produce(fd);
    _notify(SignalingWorker::NEW_CONN);
}

static void conn_io_cb(EventLoop * /*el*/, IOWatcher */*w*/, int fd, int event, void *data) {
    SignalingWorker *worker = (SignalingWorker*)data;

    if (event & EventLoop::READ) {
        worker->read_query(fd);
    }

    if (event & EventLoop::WRITE) {
        worker->write_reply(fd);
    }
}

static void conn_timer_cb(EventLoop *el, TimerWatcher * /*w*/, void *data) {
    SignalingWorker *worker = (SignalingWorker*)el->owner();
    TcpConnection *conn = (TcpConnection*)data;
    worker->process_timeout(conn);
}

void SignalingWorker::_handle_new_conn(int fd) {
    RTC_LOG(LS_INFO) << "signaling worker: " << worker_id_ << ", receive new conn, fd: " << fd;

    if (fd < 0) {
       RTC_LOG(LS_ERROR) << "signaling worker: " << worker_id_ << ", receive invalid fd: " << fd;
       return;
    }

    sock_setnoblock(fd);
    sock_setnodelay(fd);

    TcpConnection *conn = new TcpConnection(fd);
    sock_peer_to_str(fd, conn->ip, &(conn->port));
    conn->io_watcher = el_->create_io_event(conn_io_cb, this);
    el_->start_io_event(conn->io_watcher, fd, EventLoop::READ);

    conn->timer_watcher = el_->create_timer(conn_timer_cb, conn, true);
    el_->start_timer(conn->timer_watcher, 100000); // 100ms

    conn->last_interaction = el_->now();

    if ((size_t)fd > conns_.size()) {
        conns_.resize(fd *2, nullptr);
    }

    conns_[fd] = conn;

}

void SignalingWorker::read_query(int fd) {
    RTC_LOG(LS_INFO) << "signaling worker: " << worker_id_ << ", receive read event, fd: " << fd;

    if (fd < 0) {
        RTC_LOG(LS_WARNING) << "invalid fd: " << fd;
        return;
    }

    TcpConnection *conn =  conns_[fd];
    if (conn == nullptr) {
        RTC_LOG(LS_WARNING) << "tcp conntion is closed, fd: " << fd;
        return;
    }

    int nread = 0;  // 实际读出来的数据大小
    int read_len = conn->bytes_expected;  // 期待读取的数据大小
    int qb_len = sdslen(conn->querybuf);
    conn->querybuf = sdsMakeRoomFor(conn->querybuf, read_len);
    nread = sock_read_data(fd, conn->querybuf + qb_len, read_len);

    conn->last_interaction = el_->now();

    RTC_LOG(LS_INFO) << "sock read data, fd:" << fd << " , read len: " << nread;

    if (-1 == nread) {
        _close_connection(conn);
        return;
    } else if (nread > 0) {
        sdsIncrLen(conn->querybuf, nread); // 调整sds字符串conn->querybuf中len和free的大小
    }

    int ret = _process_query_buffer(conn);
    if (ret != 0) {
        _close_connection(conn);
        return;  
    }
}

int SignalingWorker::_process_query_buffer(TcpConnection *conn) {
    while (sdslen(conn->querybuf) >= conn->bytes_processed + conn->bytes_expected) {
        xhead_t *head = (xhead_t*)(conn->querybuf);
        if (TcpConnection::STATE_HEAD == conn->current_state) {
            if (XHEAD_MAGIC_NUM != head->magic_num) {
                RTC_LOG(LS_INFO) << "invalid data, fd:" << conn->fd;
                return -1;
            }

            conn->current_state = TcpConnection::STATE_BODY;
            conn->bytes_processed = XHEAD_SIZE;
            conn->bytes_expected = head->body_len; // 下一个期待的接收大小就是包体大小了。
        } else {
            rtc::Slice header(conn->querybuf, XHEAD_SIZE);
            rtc::Slice body(conn->querybuf + XHEAD_SIZE, head->body_len);

            int ret = _process_request(conn, header, body);
            if (ret != 0) {
                return -1;
            }

            // 后面的数据过来就不处理了，暂时是短连接处理
            // 长连接的需要另外再处理。。
            conn->bytes_processed = 65535;

        }
    }

    return 0;
}

int SignalingWorker::_process_request(TcpConnection *conn, const rtc::Slice &header, const rtc::Slice &body) {
    RTC_LOG(LS_INFO) << "receive body: " << body.data();

    xhead_t *xh = (xhead_t*)(header.data());

    Json::CharReaderBuilder builder;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    Json::Value root;
    JSONCPP_STRING err;
    reader->parse(body.data(), body.data() + body.size(), &root, &err);
    if (!err.empty()) {
        RTC_LOG(LS_WARNING) << "parse json body error: " << err << ", fd:" << conn->fd << ", log id:" << xh->log_id;
        return -1;
    }

    int cmdNo;
    try {
        cmdNo = root["cmdno"].asInt();
    } catch (Json::Exception e) {
        RTC_LOG(LS_WARNING) << "no cmdno field in body, log id:" << xh->log_id;
        return -1;
    }

    int ret = 0;

    switch (cmdNo) {
        case CMDNO_PUSH:
            return _process_push(cmdNo, conn, root, xh->log_id);
        case CMDNO_PULL:
            return _process_pull(cmdNo, conn, root, xh->log_id);
        case CMDNO_STOPPUSH:
            ret = _process_stop_push(cmdNo, conn, root, xh->log_id);
            break;
        case CMDNO_STOPPULL:
            ret = _process_stop_pull(cmdNo, conn, root, xh->log_id);
            break; 
        default:
            ret = -1;
            RTC_LOG(LS_WARNING) << "unknown cmdno: " << cmdNo << ", log_id: " << xh->log_id;
            break;
    }

    // 返回处理结果
    char* buf = (char*)zmalloc(XHEAD_SIZE + MAX_RES_BUF);
    xhead_t* res_xh = (xhead_t*)buf;
    memcpy(res_xh, header.data(), header.size());

    Json::Value res_root;
    if (0 == ret) {
        res_root["err_no"] = 0;
        res_root["err_msg"] = "success";
    } else {
        res_root["err_no"] = -1;
        res_root["err_msg"] = "process error";
    }

    Json::StreamWriterBuilder write_builder;
    write_builder.settings_["indentation"] = "";
    std::string json_data = Json::writeString(write_builder, res_root);
    RTC_LOG(LS_INFO) << "response body: " << json_data;   

    res_xh->body_len = json_data.size();
    snprintf(buf + XHEAD_SIZE, MAX_RES_BUF, "%s", json_data.c_str());
    
    rtc::Slice reply(buf, XHEAD_SIZE + res_xh->body_len);
    _add_reply(conn, reply);

    return ret;
}

int SignalingWorker::_process_stop_push(int cmdno, TcpConnection* /*conn*/, 
        const Json::Value& root, uint32_t log_id) 
{
    uint64_t uid;
    std::string stream_name;
    
    try {
        uid = root["uid"].asUInt64();
        stream_name = root["stream_name"].asString();
    } catch (Json::Exception e) {
        RTC_LOG(LS_WARNING) << "parse json body error: " << e.what()
            << "log_id: " << log_id;
        return -1;
    }
    
    RTC_LOG(LS_INFO) << "cmdno[" << cmdno << "] uid[" << uid 
        << "] stream_name[" << stream_name 
        << "] signaling server send stop push request";
    
    std::shared_ptr<RtcMsg> msg = std::make_shared<RtcMsg>();
    msg->cmdno = cmdno;
    msg->uid = uid;
    msg->stream_name = stream_name;
    msg->log_id = log_id;

    return g_rtc_server->send_rtc_msg(msg);
}

int SignalingWorker::_process_stop_pull(int cmdno, TcpConnection* /*c*/, const Json::Value& root, uint32_t log_id) {
    uint64_t uid;
    std::string stream_name;
    
    try {
        uid = root["uid"].asUInt64();
        stream_name = root["stream_name"].asString();
    } catch (Json::Exception e) {
        RTC_LOG(LS_WARNING) << "parse json body error: " << e.what()
            << "log_id: " << log_id;
        return -1;
    }
    
    RTC_LOG(LS_INFO) << "cmdno[" << cmdno << "] uid[" << uid 
        << "] stream_name[" << stream_name 
        << "] signaling server send stop pull request";
    
    std::shared_ptr<RtcMsg> msg = std::make_shared<RtcMsg>();
    msg->cmdno = cmdno;
    msg->uid = uid;
    msg->stream_name = stream_name;
    msg->log_id = log_id;

    return g_rtc_server->send_rtc_msg(msg);
}

void SignalingWorker::_close_connection(TcpConnection *conn) {
    RTC_LOG(LS_INFO) << "close connection, fd: " << conn->fd;
    close(conn->fd);
    _remove_connection(conn);
}

void SignalingWorker::_remove_connection(TcpConnection *conn) {
    el_->delete_timer(conn->timer_watcher);
    el_->delete_io_event(conn->io_watcher);
    conns_[conn->fd] = nullptr;
    delete conn;
    conn = nullptr;
}

void SignalingWorker::process_timeout(TcpConnection *conn) {
    if (el_->now() - conn->last_interaction >= (unsigned long)options_.connection_timeout) {
        RTC_LOG(LS_INFO) << "connection timeout, fd: " << conn->fd;
        _close_connection(conn);
    }
}

int SignalingWorker::_process_push(int cmdno, TcpConnection *conn, const Json::Value &root, uint32_t log_id) {
    uint64_t uid;
    std::string stream_name;
    int audio;
    int video;
    int dtls_on = 1;
    std::string offer;

    try {
        uid = root["uid"].asUInt64();
        stream_name = root["stream_name"].asString();
        audio = root["audio"].asInt();
        video = root["video"].asInt();
        offer = root["sdp"].asString();

        if (!root["dtls_on"].isNull()) {
            dtls_on = root["dtls_on"].asInt();
        }

    } catch (Json::Exception e) {
        RTC_LOG(LS_WARNING) << "parse json body error:"<< e.what() << ", log id:" << log_id;
        return -1;
    }

    RTC_LOG(LS_INFO) << "cmdno["<< cmdno 
            << "] uid[" << uid 
            << "] stream_name[" << stream_name 
            << "] audio[" << audio
            << "] video[" << video 
            << "] dtls_on [" << dtls_on
            << "] sdp [" << offer
            << "] signaling server push request";

    std::shared_ptr<RtcMsg> msg = std::make_shared<RtcMsg>();
    msg->cmdno = cmdno;
    msg->uid = uid;
    msg->stream_name = stream_name;
    msg->audio = audio;
    msg->video = video;
    msg->dtls_on = dtls_on;
    msg->log_id = log_id;
    msg->worker = this;
    msg->conn = conn;
    msg->fd = conn->fd;
    msg->sdp = offer;

    return g_rtc_server->send_rtc_msg(msg);
}

int SignalingWorker::_process_pull(int cmdno, TcpConnection *conn, const Json::Value &root, uint32_t log_id) {
    uint64_t uid;
    std::string stream_name;
    int audio;
    int video;
    std::string offer;

    try {
        uid = root["uid"].asUInt64();
        stream_name = root["stream_name"].asString();
        audio = root["audio"].asInt();
        video = root["video"].asInt();
        offer = root["sdp"].asString();

    } catch (Json::Exception e) {
        RTC_LOG(LS_WARNING) << "parse json body error:"<< e.what() << ", log id:" << log_id;
        return -1;
    }

    RTC_LOG(LS_INFO) << "cmdno["<< cmdno 
            << "] uid[" << uid 
            << "] stream_name[" << stream_name 
            << "] audio[" << audio
            << "] video[" << video << "] signaling server pull request";

    std::shared_ptr<RtcMsg> msg = std::make_shared<RtcMsg>();
    msg->cmdno = cmdno;
    msg->uid = uid;
    msg->stream_name = stream_name;
    msg->audio = audio;
    msg->video = video;
    msg->log_id = log_id;
    msg->worker = this;
    msg->conn = conn;
    msg->fd = conn->fd;
    msg->sdp = offer;

    return g_rtc_server->send_rtc_msg(msg);
}

void SignalingWorker::push_msg(std::shared_ptr<RtcMsg> msg) {
    std::unique_lock<std::mutex> lock(q_msg_mtx_);
    q_msg_.push(msg);
}

std::shared_ptr<RtcMsg> SignalingWorker::pop_msg() {
    std::unique_lock<std::mutex> lock(q_msg_mtx_);

    if (q_msg_.empty()) {
        return nullptr;
    }

    std::shared_ptr<RtcMsg> msg = q_msg_.front();
    q_msg_.pop();

    return msg;
}

int SignalingWorker::send_rtc_msg(std::shared_ptr<RtcMsg> msg) {
    push_msg(msg);
    return _notify(SignalingWorker::RTC_MSG);
}

void SignalingWorker::_process_rtc_msg() {
    std::shared_ptr<RtcMsg> msg = pop_msg();
    if (!msg) {
        return;
    }

    switch (msg->cmdno) {
        case CMDNO_PUSH:
        case CMDNO_PULL:
            _response_server_offer(msg);
            break;
        default:
        RTC_LOG(LS_WARNING) << "unknown cmdno["<< msg->cmdno << "], worker id:" << worker_id_;
    }
}

void SignalingWorker::_response_server_offer(std::shared_ptr<RtcMsg> msg) {
    TcpConnection *conn = (TcpConnection*)(msg->conn);
    if (!conn) {
        return;
    }

    // 1、需要检查连接是否还存在
    // 这里同时传递conn和fd，是为了避免了上一个conn关闭而新建的conn的fd一样的问题
    int fd = msg->fd;
    if (fd <= 0 || size_t(fd) >= conns_.size()) {
        return;
    }

    if (conns_[fd] != conn) {
        return;
    }

    // 2、构建响应头和响应体，并处理
    xhead_t *xh = (xhead_t*)(conn->querybuf);

    rtc::Slice header(conn->querybuf, XHEAD_SIZE);
    char *buf = (char*)zmalloc(XHEAD_SIZE + MAX_RES_BUF);
    if (!buf) {
        RTC_LOG(LS_WARNING) << "zmalloc error, log_id:" << xh->log_id <<  ", worker id:" << worker_id_;
        return;
    }

    memcpy(buf, header.data(), header.size());
    xhead_t *res_xh = (xhead_t*)buf;

    Json::Value res_root;
    res_root["err_no"] = msg->err_no;
    if (msg->err_no != 0) {
        res_root["err_msg"] = "process error";
        res_root["offer"] = "";
    } else {
        res_root["err_msg"] = "success";
        res_root["offer"] = msg->sdp;
    }

    Json::StreamWriterBuilder write_builder;
    write_builder.settings_["indentation"] = ""; // 设置默认无格式化的输出
    std::string json_data = Json::writeString(write_builder, res_root);
    RTC_LOG(LS_INFO) << "signaling worker response body:" << json_data << ", worker id:" << worker_id_;

    res_xh->body_len = json_data.size();
    snprintf(buf + XHEAD_SIZE, MAX_RES_BUF, "%s", json_data.c_str());

    rtc::Slice reply(buf, XHEAD_SIZE + res_xh->body_len);
    _add_reply(conn, reply);
}

void SignalingWorker::_add_reply(TcpConnection *conn, const rtc::Slice& reply) {
    conn->reply_list.push_back(reply);
    el_->start_io_event(conn->io_watcher, conn->fd, EventLoop::WRITE);
}

void SignalingWorker::write_reply(int fd) {
    if (fd <= 0 || size_t(fd) >= conns_.size()) {
        return;
    }

    TcpConnection *conn= conns_[fd];
    if (!conn) {
        return;
    }

    while(!conn->reply_list.empty()) {
        rtc::Slice reply = conn->reply_list.front();
        // 可能一次没有写完，需要多次写入，因为需要保存一下偏移位置cur_resp_pos
        int nwritten = sock_write_data(conn->fd, reply.data() + conn->cur_resp_pos, reply.size() - conn->cur_resp_pos);
        if (-1 == nwritten) {
            _close_connection(conn);
            return;
        } else if (0 == nwritten) {
            RTC_LOG(LS_WARNING) << "write zero bytes, fd:" << conn->fd << ", worker id:" << worker_id_;
        } else if ((nwritten + conn->cur_resp_pos) >= reply.size()) {
            // 写入完成
            conn->reply_list.pop_front();
            zfree((void*)reply.data());
            conn->cur_resp_pos = 0;
            RTC_LOG(LS_INFO) << "write finish, fd:" << conn->fd << ", worker id:" << worker_id_;
        } else {
            //没有写完
            conn->cur_resp_pos += nwritten;
        }
    }

    conn->last_interaction = el_->now();

    if (conn->reply_list.empty()) {
        el_->stop_io_event(conn->io_watcher, conn->fd, EventLoop::WRITE);
        RTC_LOG(LS_INFO) << "stop write event, fd:" << conn->fd << ", worker id:" << worker_id_;
    }
}

}
