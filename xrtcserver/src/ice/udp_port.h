/**
 * @file udp_port.h
 * @author charles
 * @brief 
*/

#ifndef  __UDP_PORT_H_
#define  __UDP_PORT_H_

#include <string>
#include <memory>
#include <map>

#include <rtc_base/socket_address.h>
#include <rtc_base/third_party/sigslot/sigslot.h>

#include "base/network.h"
#include "ice/ice_def.h"
#include "ice/ice_credentials.h"
#include "ice/candidate.h"

namespace xrtc {

class EventLoop;
class AsyncUdpSocket;
class StunMessage;
class IceConnection;

typedef std::map<rtc::SocketAddress, IceConnection*> AddressMap;

class UDPPort : public sigslot::has_slots<> {
public:
    UDPPort(EventLoop* el,
            const std::string& transport_name,
            IceCandidateComponent component,
            IceParameters ice_params);
    ~UDPPort();

    std::string ice_ufrag() { return ice_params_.ice_ufrag; }
    std::string ice_pwd() { return ice_params_.ice_pwd; }

    const std::string& transport_name() { return transport_name_; }
    IceCandidateComponent component() { return component_; }
    const rtc::SocketAddress& local_addr() { return local_addr_; } 
    const std::vector<Candidate> candidates() const { return candidates_; }

    int create_ice_candidate(Network* network, int min_port, int max_port, Candidate& c);
    bool get_stun_message(const char* data, size_t len,
            const rtc::SocketAddress& addr,
            std::unique_ptr<StunMessage>* out_msg,
            std::string* out_username);
    
    void send_binding_error_response(StunMessage* stun_msg,
            const rtc::SocketAddress& addr,
            int err_code,
            const std::string& reason);
    std::string to_string();

    IceConnection* create_connection(const Candidate& candidate);
    IceConnection* get_connection(const rtc::SocketAddress& addr);

    void create_stun_username(const std::string& remote_username, std::string* stun_attr_username);
    int send_to(const char* buf, size_t len, const rtc::SocketAddress& addr);

    sigslot::signal4<UDPPort*, const rtc::SocketAddress&, StunMessage*, const std::string&> signal_unknown_address;

private:
    void _on_read_packet(AsyncUdpSocket* socket, char* buf, size_t size,
            const rtc::SocketAddress& addr, int64_t timestamp);
    bool _parse_stun_username(StunMessage *stun_msg, std::string *local_ufrag, std::string *remote_ufrag);

private:
    EventLoop* el_;
    std::string transport_name_;
    IceCandidateComponent component_;
    IceParameters ice_params_;
    int socket_ = -1;
    rtc::SocketAddress local_addr_;
    std::vector<Candidate> candidates_;
    std::unique_ptr<AsyncUdpSocket> async_socket_;
    AddressMap connections_;
};

} // namespace xrtc

#endif  // __UDP_PORT_H_
