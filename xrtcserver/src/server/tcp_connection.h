/**
 * @file tcp_connection.h
 * @author charles
 * @brief 
*/

#ifndef __SERVER_TCP_CONNECTION_H_
#define __SERVER_TCP_CONNECTION_H_

#include <list>

#include <rtc_base/sds.h>
#include <rtc_base/slice.h>

#include "base/xhead.h"

namespace xrtc {

class IOWatcher;
class TimerWatcher;

class TcpConnection {
public:
    enum {
        STATE_HEAD = 0,
        STATE_BODY = 1
    };

    TcpConnection(int fd);
    ~TcpConnection();

public:
    int fd;
    char ip[128] = {0};
    int port = 9000;
    sds querybuf;
    size_t bytes_expected = XHEAD_SIZE;
    size_t bytes_processed = 0;
    int current_state = STATE_HEAD;
    unsigned long last_interaction = 0;
    IOWatcher *io_watcher = nullptr;
    TimerWatcher *timer_watcher = nullptr;
    std::list<rtc::Slice> reply_list;
    size_t cur_resp_pos = 0;
};

} // namespace xrtc

#endif // __SERVER_TCP_CONNECTION_H_
