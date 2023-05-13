#include <rtc_base/logging.h>

#include "base/socket.h"
#include "base/async_udp_socket.h"

namespace xrtc {

const size_t MAX_BUF_SIZE = 1500;

void async_udpsocket_io_cb(EventLoop* /*el*/, IOWatcher* /*w*/, 
        int /*fd*/, int event, void* data) 
{
    AsyncUdpSocket* udpsocket_ = (AsyncUdpSocket*)data;
    if (EventLoop::READ & event) {
        udpsocket_->recv_data();
    }

    if (EventLoop::WRITE & event) {
        udpsocket_->send_data();
    }
}

AsyncUdpSocket::AsyncUdpSocket(EventLoop *el, int socket) :
    el_(el),
    socket_(socket),
    buf_(new char[MAX_BUF_SIZE]),
    size_(MAX_BUF_SIZE)
{
    socket_watcher_ = el_->create_io_event(async_udpsocket_io_cb, this);
    el_->start_io_event(socket_watcher_, socket_, EventLoop::READ);
}

AsyncUdpSocket::~AsyncUdpSocket() {
    if (socket_watcher_) {
        el_->delete_io_event(socket_watcher_);
        socket_watcher_ = nullptr;
    }

    if (buf_) {
        delete []buf_;
        buf_ = nullptr;
    }
}

void AsyncUdpSocket::recv_data() {
    while (true) {
        struct sockaddr_in addr;
        socklen_t addr_len = sizeof(addr);
        int len = sock_recv_from(socket_, buf_, size_, (struct sockaddr*)&addr, addr_len);
        if (len <= 0) {
            return;
        }
        
        // 拿到数据包到达服务器的时间
        int64_t timestamp = sock_get_recv_timestamp(socket_);
        
        int port = ntohs(addr.sin_port);
        char ip[64] = {0};
        inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));
        rtc::SocketAddress remote_addr(ip, port);
        
        signal_read_packet(this, buf_, len, remote_addr, timestamp);
    }
}

void AsyncUdpSocket::send_data() {
    size_t len = 0;
    int sent = 0;
    while (!udp_packet_list_.empty()) {
        UdpPacketData *packet = udp_packet_list_.front();
        sockaddr_storage saddr;
        len = packet->addr().ToSockAddrStorage(&saddr);
        sent = sock_send_to(socket_, packet->data(), packet->size(), 
                MSG_NOSIGNAL, (struct sockaddr*)&saddr, len);
        if (sent < 0) {
            RTC_LOG(LS_WARNING) << "send udp packet error, remote_addr: " <<
                packet->addr().ToString();
            delete packet;
            udp_packet_list_.pop_front();
            return;
        } else if (0 == sent) {
            RTC_LOG(LS_WARNING) << "send 0 bytes, try again, remote_addr: " <<
                packet->addr().ToString();
            return;
        } else {
            delete packet;
            udp_packet_list_.pop_front();
        }
    }

    if (udp_packet_list_.empty()) {
        el_->stop_io_event(socket_watcher_, socket_, EventLoop::WRITE);
    }
}

int AsyncUdpSocket::send_to(const char* data, size_t size, const rtc::SocketAddress& addr) {
    return _add_udp_packet(data, size, addr);
}

int AsyncUdpSocket::_add_udp_packet(const char* data, size_t size, const rtc::SocketAddress& addr) {
    // 尝试发送list里面的数据
    size_t len = 0;
    int sent = 0;
    while (!udp_packet_list_.empty()) {
        UdpPacketData *packet = udp_packet_list_.front();
        sockaddr_storage saddr;
        len = packet->addr().ToSockAddrStorage(&saddr);
        sent = sock_send_to(socket_, packet->data(), packet->size(), 
                MSG_NOSIGNAL, (struct sockaddr*)&saddr, len);
        if (sent < 0) {
            RTC_LOG(LS_WARNING) << "send udp packet error, remote_addr: " <<
                packet->addr().ToString();
            delete packet;
            udp_packet_list_.pop_front();
            return -1;
        } else if (0 == sent) {
            RTC_LOG(LS_WARNING) << "send 0 bytes, try again, remote_addr: " <<
                packet->addr().ToString();
            goto SEND_AGAIN;
        } else {
            delete packet;
            udp_packet_list_.pop_front();
        }
    }

    // 发送当前数据
    sockaddr_storage saddr;
    len = addr.ToSockAddrStorage(&saddr);
    sent = sock_send_to(socket_, data, size, MSG_NOSIGNAL, (struct sockaddr*)&saddr, len);
    if (sent < 0) {
        RTC_LOG(LS_WARNING) << "send udp packet error, remote_addr: " << addr.ToString();
        return -1;
    } else if (0 == sent) {
        RTC_LOG(LS_WARNING) << "send 0 bytes, try again, remote_addr: "  << addr.ToString();
        goto SEND_AGAIN;
    }

    return sent;

SEND_AGAIN:
    UdpPacketData* packet_data = new UdpPacketData(data, size, addr);
    udp_packet_list_.push_back(packet_data);
    el_->start_io_event(socket_watcher_, socket_, EventLoop::WRITE);

    return size;   
}


} // namespace xrtc


