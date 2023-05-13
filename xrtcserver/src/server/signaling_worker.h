/**
 * @file signaling_worker.h
 * @author charles
 * @brief 
*/

#ifndef __SERVER_SIGNALING_WORKER_H_
#define __SERVER_SIGNALING_WORKER_H_

#include <memory>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>

#include <rtc_base/slice.h>
#include <json/json.h>

#include "base/lock_free_queue.h"
#include "server/signaling_server.h"
#include "xrtcserver_def.h"
#include "server/settings.h"

namespace xrtc {

class EventLoop;
class IOWatcher;
class TcpConnection;

class SignalingWorker {
public:
    enum {
        QUIT = 0,
        NEW_CONN = 1,
        RTC_MSG = 2,
    };

    SignalingWorker(int worker_id, const SignalingServerOptions& options);
    ~SignalingWorker();

public:
    int init();
    bool start();
    void stop();
    void process_notify(int msg);
    void join();
    void notify_new_conn(int fd);
    void read_query(int fd);
    void process_timeout(TcpConnection *conn);
    int send_rtc_msg(std::shared_ptr<RtcMsg> msg);
    void push_msg(std::shared_ptr<RtcMsg> msg);
    std::shared_ptr<RtcMsg> pop_msg();
    void write_reply(int fd);
    
private:
    void _quit();
    int _notify(int msg);
    void _handle_new_conn(int fd);
    int _process_query_buffer(TcpConnection *conn);
    int _process_request(TcpConnection *conn, const rtc::Slice &header, const rtc::Slice &body);
    void _close_connection(TcpConnection *conn);
    void _remove_connection(TcpConnection *conn);
    void _process_rtc_msg();
    void _response_server_offer(std::shared_ptr<RtcMsg> msg);
    void _add_reply(TcpConnection *conn, const rtc::Slice& reply);

    int _process_push(int cmdno, TcpConnection *conn, const Json::Value &root, uint32_t log_id);
    int _process_pull(int cmdno, TcpConnection *conn, const Json::Value &root, uint32_t log_id);
    int _process_stop_push(int cmdno, TcpConnection *conn, const Json::Value& root, uint32_t log_id);
    int _process_stop_pull(int cmdno, TcpConnection *conn, const Json::Value& root, uint32_t log_id); 

private:
    int worker_id_;
    SignalingServerOptions options_;
    std::unique_ptr<EventLoop> el_;

    IOWatcher *pipe_wather_ = nullptr;
    int notify_recv_fd_ = -1;
    int notify_send_fd_ = -1;

    std::unique_ptr<std::thread> thread_;
    LockFreeQueue<int> q_conn_;
    std::vector<TcpConnection*> conns_;

    std::queue<std::shared_ptr<RtcMsg>> q_msg_;
    std::mutex q_msg_mtx_;
};

} // namespace xrtc

#endif // __SERVER_SIGNALING_WORKER_H_
