/**
 * @file ice_transport_channel.h
 * @author charles
 * @brief 
*/

#ifndef  __ICE_TRANSPORT_CHANNEL_H_
#define  __ICE_TRANSPORT_CHANNEL_H_

#include <string>
#include <memory>

#include <rtc_base/socket_address.h>
#include <rtc_base/third_party/sigslot/sigslot.h>

#include "base/event_loop.h"
#include "ice/ice_def.h"
#include "ice/port_allocator.h"
#include "ice/ice_credentials.h"
#include "ice/candidate.h"
#include "ice/stun.h"
#include "ice/udp_port.h"

namespace xrtc {

class IceController;

enum class IceTransportState {
    k_new,
    k_checking,
    k_connected,
    k_completed,
    k_failed,
    k_disconnected,
    k_closed,
};

class IceTransportChannel : public sigslot::has_slots<> {
public:
    IceTransportChannel(EventLoop* el, PortAllocator *allocator, const std::string& transport_name,
            IceCandidateComponent component);
    virtual ~IceTransportChannel();

public:  
    const std::string& transport_name() { 
        return transport_name_; 
    }

    IceCandidateComponent component() const { 
        return component_; 
    }

    bool writable() const { return writable_; }
    bool receiving() const { return receiving_; }
    IceTransportState state() { return state_; }

    void set_ice_params(const IceParameters& ice_params);
    void set_remote_ice_params(const IceParameters& ice_params);
    void gathering_candidate();
    void on_check_and_ping();
    int send_packet(const char* data, size_t len);

    sigslot::signal2<IceTransportChannel*, const std::vector<Candidate>&>
        signal_candidate_allocate_done;
    sigslot::signal1<IceTransportChannel*> signal_receiving_state;
    sigslot::signal1<IceTransportChannel*> signal_writable_state;
    sigslot::signal4<IceTransportChannel*, const char*, size_t, int64_t> signal_read_packet;
    sigslot::signal1<IceTransportChannel*> signal_ice_state_changed;

private:
    void _on_unknown_address(UDPPort* port,
        const rtc::SocketAddress& addr,
        StunMessage* msg,
        const std::string& remote_ufrag);
    void _on_connection_state_change(IceConnection* conn);
    void _on_connection_destroyed(IceConnection* conn);
    void _on_read_packet(IceConnection* conn, const char* buf, size_t len, int64_t ts);

    void _add_connection(IceConnection* conn);
    void _sort_connections_and_update_state();
    void _maybe_start_pinging();
    void _ping_connection(IceConnection* conn);
    void _maybe_switch_selected_connection(IceConnection* conn);
    void _switch_selected_connection(IceConnection* conn);
    void _update_connection_states();
    void _update_state();
    void _set_receiving(bool receiving);
    void _set_writable(bool writable);
    IceTransportState _compute_ice_transport_state();

    std::string to_string();

private:
    EventLoop *el_ = nullptr;
    PortAllocator *port_allocator_ = nullptr;
    std::string transport_name_;
    IceCandidateComponent component_;
    IceParameters ice_params_;
    IceParameters remote_ice_params_;
    std::vector<Candidate> local_candidates_;
    std::vector<UDPPort*> ports_;
    std::unique_ptr<IceController> ice_controller_;
    bool start_pinging_ = false;
    TimerWatcher* ping_watcher_ = nullptr;
    int cur_ping_interval_ = WEAK_PING_INTERVAL;
    int64_t last_ping_sent_ms_ = 0;
    IceConnection *selected_connection_ = nullptr;
    bool receiving_ = false;
    bool writable_ = false;
    IceTransportState state_ = IceTransportState::k_new;
    bool had_connection_ = false;
    bool has_been_connection_ = false;
};

} // end namespace xrtc

#endif  // __ICE_TRANSPORT_CHANNEL_H_
