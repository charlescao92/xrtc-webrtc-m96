#include <rtc_base/logging.h>
#include <rtc_base/time_utils.h>

#include "ice/ice_transport_channel.h"
#include "ice/ice_connection.h"
#include "ice/ice_controller.h"

namespace xrtc {

const int PING_INTERVAL_DIFF = 5;

void ice_ping_cb(EventLoop* /*el*/, TimerWatcher* /*w*/, void* data) {
    IceTransportChannel* channel = (IceTransportChannel*)data;
    channel->on_check_and_ping();
}

IceTransportChannel::IceTransportChannel(EventLoop* el, PortAllocator *allocator, const std::string& transport_name,
        IceCandidateComponent component) :
    el_(el),
    port_allocator_(allocator),
    transport_name_(transport_name),
    component_(component),
    ice_controller_(new IceController(this))
{
    RTC_LOG(LS_INFO) << "ice transport channel created, transport_name:" << transport_name_
        << ", component:" << component_;  
    ping_watcher_ = el_->create_timer(ice_ping_cb, this, true);
}

IceTransportChannel::~IceTransportChannel() {
    if (ping_watcher_) {
        el_->delete_timer(ping_watcher_);
        ping_watcher_ = nullptr;
    }

    std::vector<IceConnection*> connections = ice_controller_->connections();
    for (auto conn : connections) {
        conn->destroy();
    }

    for (auto port : ports_) {
        delete port;
    }
    ports_.clear();

    ice_controller_.reset(nullptr);

    RTC_LOG(LS_INFO) << to_string() << ": IceTransportChannel destroy";
}

void IceTransportChannel::set_ice_params(const IceParameters& ice_params) {
    RTC_LOG(LS_INFO) << "set ICE param, transport_name: " << transport_name_
        << ", component: " << component_
        << ", ufrag: " << ice_params.ice_ufrag
        << ", pwd: " << ice_params.ice_pwd;
    ice_params_ = ice_params;
}

void IceTransportChannel::set_remote_ice_params(const IceParameters& ice_params) {
    RTC_LOG(LS_INFO) << "set remote ICE param, transport_name: " << transport_name_
        << ", component: " << component_
        << ", ufrag: " << ice_params.ice_ufrag
        << ", pwd: " << ice_params.ice_pwd;

    remote_ice_params_ = ice_params;

    for (auto conn : ice_controller_->connections()) {
        conn->maybe_set_remote_ice_params(ice_params);
    }

    _sort_connections_and_update_state();
}

void IceTransportChannel::gathering_candidate() {
    // 1、先检查ice_ufrag和ice_pwd
    if (ice_params_.ice_ufrag.empty() || ice_params_.ice_pwd.empty()) {
        RTC_LOG(LS_WARNING) << "cannot gathering candidate because ICE param is empty"
            << ", transport_name: " << transport_name_
            << ", component: " << component_
            << ", ufrag: " << ice_params_.ice_ufrag
            << ", pwd: " << ice_params_.ice_pwd;
        return;
    }

    // 2、获取所有网络接口信息，每个网络信息都创建UDPPort
    auto network_list = port_allocator_->get_networks();
    if (network_list.empty()) {
        RTC_LOG(LS_WARNING) << "cannot gathering candidate because network_list is empty"
            << ", transport_name: " << transport_name_
            << ", component: " << component_;
        return;
    }

    for (auto network : network_list) {
        UDPPort* port = new UDPPort(el_, transport_name_, component_, ice_params_);
        port->signal_unknown_address.connect(this, &IceTransportChannel::_on_unknown_address);     
        ports_.push_back(port); 
        Candidate c;
        int ret = port->create_ice_candidate(network, port_allocator_->min_port(), port_allocator_->max_port(), c);
        if (ret != 0) {
            continue;
        }

        local_candidates_.push_back(c);
    }

    signal_candidate_allocate_done(this, local_candidates_);
}

void IceTransportChannel::_on_unknown_address(UDPPort* port,
        const rtc::SocketAddress& addr,
        StunMessage* msg,
        const std::string& remote_ufrag)
{
    const StunUInt32Attribute *priority_attr = msg->get_uint32(STUN_ATTR_PRIORITY);
    if (!priority_attr) {
        RTC_LOG(LS_WARNING) << to_string() << ": priority not found in the"
            << " binding request message, remote_addr: " << addr.ToString();
        port->send_binding_error_response(msg, addr, STUN_ERROR_BAD_REQUEST, STUN_ERROR_REASON_BAD_REQUEST);            
        return;
    }

    uint32_t remote_priority = priority_attr->value();

    // 开始创建peer反射的candidate
    Candidate remote_candidate;
    remote_candidate.component = component_;
    remote_candidate.protocol = "udp";
    remote_candidate.address = addr;
    remote_candidate.username = remote_ufrag;
    remote_candidate.password = remote_ice_params_.ice_pwd;
    remote_candidate.priority = remote_priority;
    remote_candidate.type = PRFLX_PORT_TYPE;

    RTC_LOG(LS_INFO) << to_string() << ": create peer reflexive candidate: "
        << remote_candidate.to_string();

    IceConnection* conn = port->create_connection(remote_candidate);
    if (!conn) {
        RTC_LOG(LS_WARNING) << to_string() << ": create connection from "
            << " peer reflexive candidate error, remote_addr: "
            << addr.ToString();
        port->send_binding_error_response(msg, addr, STUN_ERROR_SERVER_ERROR,
                STUN_ERROR_REASON_SERVER_ERROR);
        return;
    }

    RTC_LOG(LS_INFO) << to_string() << ": create connection from "
        << " peer reflexive candidate success, remote_addr: "
        << addr.ToString();
    
    _add_connection(conn);

    conn->handle_stun_binding_request(msg);

    _sort_connections_and_update_state();
}

void IceTransportChannel::_add_connection(IceConnection* conn) {
    conn->signal_state_change.connect(this, 
            &IceTransportChannel::_on_connection_state_change);
    conn->signal_connection_destroy.connect(this,
            &IceTransportChannel::_on_connection_destroyed);
    conn->signal_read_packet.connect(this,
            &IceTransportChannel::_on_read_packet);

    had_connection_ = true;
    
    ice_controller_->add_connection(conn);
}

void IceTransportChannel::_on_read_packet(IceConnection* /*conn*/,
        const char* buf, size_t len, int64_t ts)
{
    signal_read_packet(this, buf, len, ts);
}

void IceTransportChannel::_on_connection_destroyed(IceConnection* conn) {
    ice_controller_->on_connection_destroyed(conn);
    RTC_LOG(LS_INFO) << to_string() << ": Remove connection: " << conn
        << " with " << ice_controller_->connections().size() << " remaining";
    if (selected_connection_ == conn) {
        RTC_LOG(LS_INFO) << to_string() 
            << ": Selected connection destroyed, should select a new connection";
        _switch_selected_connection(nullptr);
        _sort_connections_and_update_state();
    } else {
        _update_state();
    }
}

void IceTransportChannel::_switch_selected_connection(IceConnection* conn) {
    IceConnection* old_selected_connection = selected_connection_;
    selected_connection_ = conn;
    if (old_selected_connection) {
        old_selected_connection->set_selected(false);
        RTC_LOG(LS_INFO) << to_string() << ": Previous connection: "
            << old_selected_connection->to_string();
    }

    if (selected_connection_) {
        RTC_LOG(LS_INFO) << to_string() << ": New selected connection: "
            << conn->to_string();

        selected_connection_->set_selected(true);
        ice_controller_->set_selected_connection(selected_connection_); 
    } else {
        RTC_LOG(LS_INFO) << to_string() << ": No connection selected";
    }
}

void IceTransportChannel::_on_connection_state_change(IceConnection* /*conn*/) {
    _sort_connections_and_update_state();
}

void IceTransportChannel::_sort_connections_and_update_state() {
    _maybe_switch_selected_connection(ice_controller_->sort_and_switch_connection());
    
    _update_state();

    _maybe_start_pinging();
}

void IceTransportChannel::_maybe_switch_selected_connection(IceConnection* conn) {
    if (!conn) {
        return;
    }
    _switch_selected_connection(conn);
}

void IceTransportChannel::_maybe_start_pinging() {
    if (start_pinging_) {
        return;
    }

    if (ice_controller_->has_pingable_connection()) {
        RTC_LOG(LS_INFO) << to_string() << ": Have a pingable connection"
            << " for the first time, starting to ping";
        
        // 启动定时器
        el_->start_timer(ping_watcher_, cur_ping_interval_ * 1000);
        start_pinging_ = true;
    }
}


std::string IceTransportChannel::to_string() {
    std::stringstream ss;
    ss << "Channel[" << this << ":" << transport_name_ << ":" << component_
        << "]";
    return ss.str();
}

void IceTransportChannel::on_check_and_ping() {
    _update_connection_states();

    auto result = ice_controller_->select_connection_to_ping(last_ping_sent_ms_ - PING_INTERVAL_DIFF);
  
    // RTC_LOG(LS_WARNING) << "===========conn: " << result.conn << ", ping interval: "
    //     << result.ping_interval;

    if (result.conn) {
        IceConnection* conn = (IceConnection*)result.conn;
        _ping_connection(conn);
        ice_controller_->mark_connection_pinged(conn);
    }

    if (cur_ping_interval_ != result.ping_interval) {
        cur_ping_interval_= result.ping_interval;
        el_->stop_timer(ping_watcher_);
        el_->start_timer(ping_watcher_, cur_ping_interval_ * 1000); 
    }
}

void IceTransportChannel::_update_connection_states() {
    std::vector<IceConnection*> connections = ice_controller_->connections();
    int64_t now = rtc::TimeMillis();
    for (auto conn : connections) {
        conn->update_state(now);
    }
}

void IceTransportChannel::_ping_connection(IceConnection* conn) {
    last_ping_sent_ms_ = rtc::TimeMillis();
    conn->ping(last_ping_sent_ms_);
}

void IceTransportChannel::_set_writable(bool writable) {
    if (writable_ == writable) {
        return;
    }

    if (writable) {
        has_been_connection_ = true;
    }

    writable_ = writable;
    RTC_LOG(LS_INFO) << to_string() << ": Change writable to " << writable_;
    signal_writable_state(this);
}

void IceTransportChannel::_set_receiving(bool receiving) {
    if (receiving_ == receiving) {
        return;
    }

    receiving_ = receiving;
    RTC_LOG(LS_INFO) << to_string() << ": Change receiving to " << receiving_;
    signal_receiving_state(this);
}

void IceTransportChannel::_update_state() {
    bool writable = selected_connection_ && selected_connection_->writable();
    _set_writable(writable);

    bool receiving = false;
    for (auto conn : ice_controller_->connections()) {
        if (conn->receiving()) {
            receiving = true;
            break;
        }
    }
    _set_receiving(receiving);

    IceTransportState state = _compute_ice_transport_state();
    if (state != state_) {
        state_ = state;
        signal_ice_state_changed(this);
    }
}

IceTransportState IceTransportChannel::_compute_ice_transport_state() {
    bool has_connection = false;
    for (auto conn : ice_controller_->connections()) {
        if (conn->active()) {
            has_connection = true;
            break;
        }
    }

    if (had_connection_ && !has_connection) {
        return IceTransportState::k_failed;
    }

    if (has_been_connection_ && !writable()) {
        return IceTransportState::k_disconnected;
    }

    if (!had_connection_ && !has_connection) {
        return IceTransportState::k_new;
    }
    
    if (has_connection && !writable()) {
        return IceTransportState::k_checking;
    }

    return IceTransportState::k_connected;
}

int IceTransportChannel::send_packet(const char* data, size_t len) {
    if (!ice_controller_->ready_to_send(selected_connection_)) {
        RTC_LOG(LS_WARNING) << to_string() << ": Selected connection not ready to send.";
        return -1;
    }

    int sent = selected_connection_->send_packet(data, len);
    if (sent <= 0) {
        RTC_LOG(LS_WARNING) << to_string() << ": Selected connection send failed.";
    }

    return sent;
}

}
