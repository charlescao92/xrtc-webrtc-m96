#include <sstream>

#include <rtc_base/logging.h>
#include <rtc_base/crc32.h>
#include <rtc_base/string_encode.h>

#include "base/event_loop.h"
#include "base/socket.h"
#include "base/async_udp_socket.h"
#include "ice/udp_port.h"
#include "ice/stun.h"
#include "ice/ice_connection.h"
#include "server/settings.h"

namespace xrtc {

UDPPort::UDPPort(EventLoop* el,
        const std::string& transport_name,
        IceCandidateComponent component,
        IceParameters ice_params) :
    el_(el),
    transport_name_(transport_name),
    component_(component),
    ice_params_(ice_params)
{
}

UDPPort::~UDPPort() {
}

std::string compute_foundation(const std::string& type,
        const std::string& protocol,
        const std::string& relay_protocol,
        const rtc::SocketAddress& base)
{
    std::stringstream ss;
    ss << type << base.HostAsURIString() << protocol << relay_protocol;
    return std::to_string(rtc::ComputeCrc32(ss.str()));
}

int UDPPort::create_ice_candidate(Network* network, int min_port, int max_port, Candidate& c) {
    socket_ = create_udp_socket(network->ip().family());
    if (socket_ < 0) {
        return -1;
    }

    if (sock_setnoblock(socket_) != 0) {
        return -1;
    }

    sockaddr_in addr_in;
    addr_in.sin_family = network->ip().family();
    addr_in.sin_addr = network->ip().ipv4_address();
    if (sock_bind(socket_, (struct sockaddr*)&addr_in, sizeof(sockaddr), 
            min_port, max_port) != 0)
    {
        return -1;
    }

    int port = 0;
    if (sock_get_address(socket_, nullptr, &port) != 0) {
        return -1;
    }

    local_addr_.SetIP(network->ip());
    local_addr_.SetIP(Singleton<Settings>::Instance()->CandidateIp().c_str());
    local_addr_.SetPort(port);

    async_socket_ = std::make_unique<AsyncUdpSocket>(el_, socket_);
    async_socket_->signal_read_packet.connect(this,  &UDPPort::_on_read_packet);

    RTC_LOG(LS_INFO) << "prepared socket address: " << local_addr_.ToString();

    c.component = component_;
    c.protocol = "udp";
    c.address = local_addr_;
    c.port = port;
    c.priority = c.get_priority(ICE_TYPE_PREFERENCE_HOST, 0, 0);
    c.username = ice_params_.ice_ufrag;
    c.password = ice_params_.ice_pwd;
    c.type = LOCAL_PORT_TYPE;
    c.foundation = compute_foundation(c.type, c.protocol, "", c.address);
    
    candidates_.push_back(c);

    return 0;
}

IceConnection* UDPPort::create_connection(const Candidate& remote_candidate)
{
    IceConnection* conn = new IceConnection(el_, this, remote_candidate);
    auto ret = connections_.insert(
            std::make_pair(conn->remote_candidate().address, conn));
    if (ret.second == false && ret.first->second != conn) {
        RTC_LOG(LS_WARNING) << to_string() << ": create ice connection on "
            << "an existing remote address, addr: " 
            << conn->remote_candidate().address.ToString();
        ret.first->second = conn;

        //todo 清理以前存在的ice connection
    }

    return conn;
}

int UDPPort::send_to(const char* buf, size_t len, const rtc::SocketAddress& addr) {
    if (!async_socket_) {
        return -1;
    }

    return async_socket_->send_to(buf, len, addr);
}

IceConnection* UDPPort::get_connection(const rtc::SocketAddress& addr) {
    auto iter = connections_.find(addr);
    return iter == connections_.end() ? nullptr : iter->second;
}

void UDPPort::_on_read_packet(AsyncUdpSocket* /*socket*/, char* buf, size_t size,
        const rtc::SocketAddress& addr, int64_t timestamp)
{
    if (IceConnection *conn = get_connection(addr)) {
        conn->on_read_packet(buf, size, timestamp);
        return;
    }

    std::unique_ptr<StunMessage> stun_msg;
    std::string remote_ufrag;
    bool res = get_stun_message(buf, size, addr, &stun_msg, &remote_ufrag);
    if (!res || !stun_msg) {
        return;
    }

    if (STUN_BINDING_REQUEST == stun_msg->type()) {
        RTC_LOG(LS_INFO) << to_string() << ": Received "
            << stun_method_to_string(stun_msg->type())
            << " id=" << rtc::hex_encode(stun_msg->transaction_id())
            << " from " << addr.ToString();
        signal_unknown_address(this, addr, stun_msg.get(), remote_ufrag);
    }  
}

bool UDPPort::get_stun_message(const char* data, size_t len,        
        const rtc::SocketAddress& addr,
        std::unique_ptr<StunMessage>* out_msg,     
        std::string* out_username)
{
    // 先验证fingerprint
    if (!StunMessage::validate_fingerprint(data, len)) {
        return false;
    }

    std::unique_ptr<StunMessage> stun_msg = std::make_unique<StunMessage>();
    rtc::ByteBufferReader buf(data, len);
    if (!stun_msg->read(&buf) || buf.Length() != 0) {
        return false;
    }

    if (STUN_BINDING_REQUEST == stun_msg->type()) {
        if (!stun_msg->get_byte_string(STUN_ATTR_USERNAME) ||
                !stun_msg->get_byte_string(STUN_ATTR_MESSAGE_INTEGRITY))
        {
            RTC_LOG(LS_WARNING) << to_string() << ": recevied "
                << stun_method_to_string(stun_msg->type())
                << " without username/M-I attr from "
                << addr.ToString();
            send_binding_error_response(stun_msg.get(), addr, STUN_ERROR_BAD_REQUEST,
                    STUN_ERROR_REASON_BAD_REQUEST);
            return true;
        }

        // 解析并验证USERNAME属性
        std::string local_ufrag;
        std::string remote_ufrag;
        if (!_parse_stun_username(stun_msg.get(), &local_ufrag, &remote_ufrag) ||
            local_ufrag != ice_params_.ice_ufrag) 
        {
            RTC_LOG(LS_WARNING) << to_string() << ": recevied "
                << stun_method_to_string(stun_msg->type())
                << " with bad local_ufrag: " << local_ufrag
                << " from " << addr.ToString();
            send_binding_error_response(stun_msg.get(), addr, STUN_ERROR_UNAUTHORIZED,
                    STUN_ERROR_REASON_UNAUTHORIZED);
            return true;
        }

        // 解析并验证MESSAGE-INTEGRITY属性
        if (stun_msg->validate_message_integrity(ice_params_.ice_pwd) !=
                StunMessage::IntegrityStatus::k_integrity_ok)
        {
            RTC_LOG(LS_WARNING) << to_string() << ": recevied "
                << stun_method_to_string(stun_msg->type())
                << " with bad M-I from "
                << addr.ToString();
            send_binding_error_response(stun_msg.get(), addr, STUN_ERROR_UNAUTHORIZED,
                    STUN_ERROR_REASON_UNAUTHORIZED);
            return true;
        }

        *out_username = remote_ufrag;
    }
    
    *out_msg = std::move(stun_msg);

    return true;
}

bool UDPPort::_parse_stun_username(StunMessage *stun_msg, std::string *local_ufrag, std::string *remote_ufrag) {
    local_ufrag->clear();
    remote_ufrag->clear();

    const StunByteStringAttribute* attr = stun_msg->get_byte_string(STUN_ATTR_USERNAME);
    if (!attr) {
        return false;
    }

    //RFRAG:LFRAG
    std::string username = attr->get_string();
    std::vector<std::string> fields;
    rtc::split(username, ':', &fields);
    if (fields.size() != 2) {
        return false;
    }

    *local_ufrag = fields[0];
    *remote_ufrag = fields[1];
    
    return true;
}

std::string UDPPort::to_string() {
    std::stringstream ss;
    ss << "Port[" << this << ":" << transport_name_ << ":" << component_
        << ":" << ice_params_.ice_ufrag << ":" << ice_params_.ice_pwd
        << ":" << local_addr_.ToString() << "]";
    return ss.str();
}

void UDPPort::send_binding_error_response(StunMessage* stun_msg,
        const rtc::SocketAddress& addr,
        int err_code,
        const std::string& reason)
{
    if (!async_socket_) {
        return;
    }

    // 1、构建错误响应的StunMessage
    StunMessage response;
    response.set_type(STUN_BINDING_ERROR_RESPONSE);
    response.set_transaction_id(stun_msg->transaction_id());
    auto error_attr = StunAttribute::create_error_code();
    error_attr->set_code(err_code);
    error_attr->set_reason(reason);
    response.add_attribute(std::move(error_attr));

    if (err_code != STUN_ERROR_BAD_REQUEST && err_code != STUN_ERROR_UNAUTHORIZED) {
        response.add_message_integrity(ice_params_.ice_pwd);
    }

    response.add_fingerprint();

    // 2、将StunMessage转换为发送的buf
    rtc::ByteBufferWriter buf;
    if (!response.write(&buf)) {
        return;
    }

    // 3、将转换后的buf发送出去
    int ret = async_socket_->send_to(buf.Data(), buf.Length(), addr);
    if (ret < 0) {
        RTC_LOG(LS_WARNING) << to_string() << " send "
            << stun_method_to_string(response.type())
            << " error, ret=" << ret
            << ", to=" << addr.ToString();
    } else {
        RTC_LOG(LS_INFO) << to_string() << " send "
            << stun_method_to_string(response.type())
            << " success, reason=" << reason
            << ", to=" << addr.ToString();
    }
}

void UDPPort::create_stun_username(const std::string& remote_username, 
        std::string* stun_attr_username)
{
    stun_attr_username->clear();
    *stun_attr_username = remote_username;
    stun_attr_username->append(":");
    stun_attr_username->append(ice_params_.ice_ufrag);
}

}
