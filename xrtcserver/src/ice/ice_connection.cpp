#include <sstream>

#include <rtc_base/logging.h>
#include <rtc_base/time_utils.h>
#include <rtc_base/helpers.h>

#include "ice/ice_connection.h"
#include "ice/udp_port.h"

namespace xrtc {

// 意思是 old_rtt : new_rtt = 3 : 1
const int RTT_RATIO = 3;
const int MIN_RTT = 100;
const int MAX_RTT = 60000;

ConnectionRequest::ConnectionRequest(IceConnection* conn) :
    StunRequest(new StunMessage()), connection_(conn) 
{
    ice_tiebreaker_ = rtc::CreateRandomId64();
}

void ConnectionRequest::prepare(StunMessage* msg) {
    msg->set_type(STUN_BINDING_REQUEST);
    std::string username;
    connection_->port()->create_stun_username(connection_->remote_candidate().username, &username);
    msg->add_attribute(std::make_unique<StunByteStringAttribute>(STUN_ATTR_USERNAME, username));
    //msg->add_attribute(std::make_unique<StunUInt64Attribute>(STUN_ATTR_ICE_CONTROLLING, ice_tiebreaker_));
    msg->add_attribute(std::make_unique<StunUInt64Attribute>(STUN_ATTR_ICE_CONTROLLED, ice_tiebreaker_));
    msg->add_attribute(std::make_unique<StunByteStringAttribute>(STUN_ATTR_USE_CANDIDATE, 0));

    // priority
    int type_pref = ICE_TYPE_PREFERENCE_PRFLX;
    uint32_t prflx_priority = (type_pref << 24) | (connection_->local_candidate().priority & 0x00FFFFFF);
    msg->add_attribute(std::make_unique<StunUInt32Attribute>(STUN_ATTR_PRIORITY, prflx_priority));
    msg->add_message_integrity(connection_->remote_candidate().password);
    msg->add_fingerprint();        
}

void ConnectionRequest::on_request_response(StunMessage* msg) {
    connection_->on_connection_request_response(this, msg);
}

void ConnectionRequest::on_request_error_response(StunMessage* msg) {
    connection_->on_connection_request_error_response(this, msg);
}

IceConnection::IceConnection(EventLoop *el, 
    UDPPort *port, 
    const Candidate& remote_candidate) :
    el_(el),
    port_(port),
    remote_candidate_(remote_candidate)
{
    requests_.signal_send_packet.connect(this, &IceConnection::_on_stun_send_packet);
}

IceConnection::~IceConnection() {
}

void IceConnection::_on_stun_send_packet(StunRequest* request, const char* buf, size_t len) {
    int ret = port_->send_to(buf, len, remote_candidate_.address);
    if (ret < 0) {
        RTC_LOG(LS_WARNING) << to_string() << ": Failed to send STUN binding request: ret="
            << ret << ", id=" << rtc::hex_encode(request->id());
    }
}

void IceConnection::on_read_packet(const char* buf, size_t len, int64_t timestamp) {
    std::unique_ptr<StunMessage> stun_msg;
    std::string remote_ufrag;
    const Candidate& remote = remote_candidate_;
    if (!port_->get_stun_message(buf, len, remote.address, &stun_msg, &remote_ufrag)) {
        // 这个不是stun包，可能是其它的比如dtls或者rtp包
        signal_read_packet(this, buf, len, timestamp); 
    } else if (!stun_msg) {    
    } else { // stun massage
        switch (stun_msg->type()) {
            case STUN_BINDING_REQUEST:
                if (remote_ufrag != remote.username) {
                    RTC_LOG(LS_WARNING) << to_string() << ": Received "
                        << stun_method_to_string(stun_msg->type())
                        << " with bad username=" << remote_ufrag
                        << ", transaction_id=" << rtc::hex_encode(stun_msg->transaction_id());
                    port_->send_binding_error_response(stun_msg.get(),
                            remote.address, STUN_ERROR_UNAUTHORIZED,
                            STUN_ERROR_REASON_UNAUTHORIZED);
                } else {
                    RTC_LOG(LS_INFO) << to_string() << ": Received "
                        << stun_method_to_string(stun_msg->type())
                        << ", transaction_id=" << rtc::hex_encode(stun_msg->transaction_id());
                    handle_stun_binding_request(stun_msg.get());
                }
                break;
            case STUN_BINDING_RESPONSE:
            case STUN_BINDING_ERROR_RESPONSE:
                stun_msg->validate_message_integrity(remote_candidate_.password);
                if (stun_msg->integrity_ok()) {
                    requests_.check_response(stun_msg.get());
                }
                break;
            default:
                break;
        }  
    }
}

// rfc5245
// g : controlling candidate priority
// d : controlled candidate priority
// conn priority = 2^32 * min(g, d) + 2 * max(g, d) + (g > d ? 1 : 0)
uint64_t IceConnection::priority() {
    uint32_t g = local_candidate().priority;
    uint32_t d = remote_candidate().priority;
    uint64_t priority = std::min(g, d);
    priority = priority << 32;
    return priority + 2 * std::max(g, d) + (g > d ? 1 : 0);
}

void IceConnection::handle_stun_binding_request(StunMessage* stun_msg) {
    // role的冲突问题(控制方和被控制方)，当前xrtcserver强制服务端和客户端都是controling控制方，所以不存在冲突问题
    
    // 发送binding response
    send_stun_binding_response(stun_msg);
}

void IceConnection::send_stun_binding_response(StunMessage* stun_msg) {
    const StunByteStringAttribute* username_attr = stun_msg->get_byte_string(
            STUN_ATTR_USERNAME);
    if (!username_attr) {
        RTC_LOG(LS_WARNING) << "send stun binding response error: no username";
        return;
    }

    StunMessage response;
    response.set_type(STUN_BINDING_RESPONSE);
    response.set_transaction_id(stun_msg->transaction_id());
    // 4 + 8
    response.add_attribute(std::make_unique<StunXorAddressAttribute>
            (STUN_ATTR_XOR_MAPPED_ADDRESS, remote_candidate().address));
    // 4 + 20
    response.add_message_integrity(port_->ice_pwd());
    // 4 + 4
    response.add_fingerprint();

    _send_response_message(response);
}

void IceConnection::_send_response_message(const StunMessage& response) {
    const rtc::SocketAddress &addr = remote_candidate_.address;

    rtc::ByteBufferWriter buf;
    if (!response.write(&buf)) {
        return;
    }

    int ret = port_->send_to(buf.Data(), buf.Length(), addr);
    if (ret < 0) {
        RTC_LOG(LS_WARNING) << to_string() << ": Send "
            << stun_method_to_string(response.type())
            << " error, to=" << addr.ToString()
            << ", transaction id:" << rtc::hex_encode(response.transaction_id());
        return;
    }

    RTC_LOG(LS_INFO) << to_string() << ": Send "
        << stun_method_to_string(response.type())
        << " to=" << addr.ToString()
        << ", transaction id:" << rtc::hex_encode(response.transaction_id());

}

bool IceConnection::stable(int64_t now) const {
    return rtt_samples_ > RTT_RATIO + 1 && !_miss_response(now);
}

bool IceConnection::_miss_response(int64_t now) const {
    if (pings_since_last_response_.empty()) {
        return false;
    }

    int waiting = now - pings_since_last_response_[0].sent_time;
    return waiting > 2 * rtt_;
}

void IceConnection::ping(int64_t now) {
    last_ping_sent_ = now;
    ConnectionRequest* request = new ConnectionRequest(this);
    pings_since_last_response_.push_back(SentPing(request->id(), now));
    RTC_LOG(LS_INFO) << to_string() << ": Sending STUN ping, id=" << rtc::hex_encode(request->id());
    requests_.send(request);
    set_state(IceCandidatePairState::IN_PROGRESS);
    num_pings_sent_++;
}

std::string IceConnection::to_string() {
    std::stringstream ss;
    ss << "Conn[" << this << "] " << port_->transport_name() 
        << ":" << port_->component() 
        << ":" << port_->local_addr().ToString()
        << "->" << remote_candidate_.address.ToString();
    return ss.str();
}

void IceConnection::maybe_set_remote_ice_params(const IceParameters& ice_params) {
    if (remote_candidate_.username == ice_params.ice_ufrag &&
            remote_candidate_.password.empty())
    {
        remote_candidate_.password = ice_params.ice_pwd;
    }
}

const Candidate& IceConnection::local_candidate() const {
    return port_->candidates()[0];
}

void IceConnection::print_pings_since_last_response(std::string& pings, size_t max) {
    std::stringstream ss;
    if (pings_since_last_response_.size() > max) {
        for (size_t i = 0; i < max; ++i) {
            ss << rtc::hex_encode(pings_since_last_response_[i].id) << " ";
        }
        ss << "... " << (pings_since_last_response_.size() - max) << " more";
    } else {
        for (auto ping : pings_since_last_response_) {
            ss << rtc::hex_encode(ping.id) << " ";
        }
    }
    pings = ss.str();
}

int64_t IceConnection::last_received() {
    return std::max(std::max(last_ping_received_, last_ping_response_received_), last_data_received_);
}

int IceConnection::receiving_timeout() {
    return WEAK_CONNECTION_RECEIVE_TIMEOUT;
}

void IceConnection::update_receiving(int64_t now) {
    bool receiving = false; // 是否是可读状态
    if (last_ping_sent_ < last_ping_response_received_) {
        receiving = true;
    } else {
        // 如果曾经收到数据，并且当前时间小时超时时间，也算是可读状态，否则不可读
        receiving = last_received() > 0 && 
            (now < last_received() + receiving_timeout());
    }

    if (receiving_ == receiving) {
        return;
    }

    if (receiving) {
        RTC_LOG(LS_INFO) << to_string() << ": Set receiving to STATE_WRITABLE";
    } else {
        RTC_LOG(LS_INFO) << to_string() << ": Set receiving to STATE_WRITE_UNRELIABLE";
    }

    receiving_ = receiving;
    signal_state_change(this);  
}

void IceConnection::set_write_state(WriteState state) {
    WriteState old_state = write_state_;
    write_state_ = state;
    if (old_state != state) {
        RTC_LOG(LS_INFO) << to_string() << ": Set write state from " << old_state
            << " to " << state;
        signal_state_change(this);
    }
}

void IceConnection::received_ping_response(int rtt) {
    // old_rtt : new_rtt 的权重比值 = 3 : 1 
    // rtt值序列：5 10 20
    // 默认rtt = 5
    // rtc::GetNextMovingAverage计算的值：rtt = 5 * 0.75 + 10 * 0.25 = 3.75 + 2.5 = 6.25
    // rtc::GetNextMovingAverage的作用是避免突然的网络剧烈抖动对rtt的值变化太大
    if (rtt_samples_ > 0) {
        rtt_ = rtc::GetNextMovingAverage(rtt_, rtt, RTT_RATIO);
    } else {
        rtt_ = rtt;
    }

    ++rtt_samples_;
    
    last_ping_response_received_ = rtc::TimeMillis();
    pings_since_last_response_.clear();
    update_receiving(last_ping_response_received_);
    set_write_state(STATE_WRITABLE);
    set_state(IceCandidatePairState::SUCCEEDED);
}

void IceConnection::on_connection_request_response(ConnectionRequest* request, StunMessage* msg) {
    int rtt = request->elapsed();
    std::string pings;
    //  收到这个响应之前还有多少个ping的请求没有收到
    print_pings_since_last_response(pings, 5);
    RTC_LOG(LS_INFO) << to_string() << ": Received "
        << stun_method_to_string(msg->type())
        << ", id=" << rtc::hex_encode(msg->transaction_id())
        << ", rtt=" << rtt
        << ", pings=" << pings;

    received_ping_response(rtt); 
}

void IceConnection::_fail_and_destroy() {
    set_state(IceCandidatePairState::FAILED);
    destroy();
}

void IceConnection::destroy() {
    RTC_LOG(LS_INFO) << to_string() << ": Connection destroyed";
    signal_connection_destroy(this);
    delete this;
}

void IceConnection::on_connection_request_error_response(ConnectionRequest* request, StunMessage* msg) {
    int rtt = request->elapsed();
    int error_code = msg->get_error_code_value();
    RTC_LOG(LS_WARNING) << to_string() << ": Received: "
        << stun_method_to_string(msg->type())
        << ", id=" << rtc::hex_encode(msg->transaction_id())
        << ", rtt=" << rtt
        << ", code=" << error_code;

    if (STUN_ERROR_UNAUTHORIZED == error_code ||
            STUN_ERROR_UNKNOWN_ATTRIBUTE == error_code ||
            STUN_ERROR_SERVER_ERROR == error_code)
    {
        // retry maybe recover
    } else {
        _fail_and_destroy();
    }
}

void IceConnection::set_state(IceCandidatePairState state) {
    if (state_ != state) {
        RTC_LOG(LS_INFO) << to_string() << ": Set state " << state_ << "->" << state;
        state_ = state;
    }
}

bool IceConnection::_too_many_ping_fails(size_t max_pings, int rtt, int64_t now) {
    if (pings_since_last_response_.size() < max_pings) {
        return false;
    }

    int expected_response_time = pings_since_last_response_[max_pings-1].sent_time + rtt;
    return now > expected_response_time;
}

bool IceConnection::_too_long_without_response(int min_time, int64_t now) {
    if (pings_since_last_response_.empty()) {
        return false;
    }

    return now > pings_since_last_response_[0].sent_time + min_time;
}

void IceConnection::update_state(int64_t now) {
    int rtt = 2 * rtt_;
    if (rtt < MIN_RTT) {
        rtt = MIN_RTT;
    } else if (rtt > MAX_RTT) {
        rtt = MAX_RTT;
    }

    if (write_state_ == STATE_WRITABLE &&
            _too_many_ping_fails(CONNECTION_WRITE_CONNECT_FAILS, rtt, now) &&
            _too_long_without_response(CONNECTION_WRITE_CONNECT_TIMEOUT, now))
    {
        RTC_LOG(LS_INFO) << to_string() << ": Unwritable after "
            << CONNECTION_WRITE_CONNECT_FAILS << " ping fails and "
            << now - pings_since_last_response_[0].sent_time
            << "ms without a response";

        set_write_state(STATE_WRITE_UNRELIABLE);
    }

    if ((write_state_ == STATE_WRITE_UNRELIABLE || write_state_ == STATE_WRITE_INIT) &&
            _too_long_without_response(CONNECTION_WRITE_TIMEOUT, now))
    {
        RTC_LOG(LS_INFO) << to_string() << ": Timeout after "
            << now - pings_since_last_response_[0].sent_time
            << "ms without a response";

        set_write_state(STATE_WRITE_TIMEOUT);
    }

    update_receiving(now);
}

int IceConnection::send_packet(const char* data, size_t len) {
    if (!port_) {
        return -1;
    }

    return port_->send_to(data, len, remote_candidate_.address);
}

} // end namespace xrtc
