#include <rtc_base/zmalloc.h>

#include "tcp_connection.h"

namespace xrtc {

TcpConnection::TcpConnection(int fd) : 
    fd(fd),
    querybuf(sdsempty()) {

}

TcpConnection::~TcpConnection() {
    sdsfree(querybuf);
    
    while(!reply_list.empty()) {
        rtc::Slice reply = reply_list.front();
        zfree((void*)reply.data());
        reply_list.pop_front();
    }
    reply_list.clear();
}

}