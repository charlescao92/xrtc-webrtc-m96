#include <algorithm>

#include <rtc_base/logging.h>

#include "base/event_loop.h"
#include "ice/ice_agent.h"
#include "ice/candidate.h"

namespace xrtc {

IceAgent::IceAgent(EventLoop* el, PortAllocator *allocator) : 
    el_(el),
    port_allocator_(allocator) 
{

}

IceAgent::~IceAgent() {
    for (auto channel : channels_) {
        delete channel;
    }

    channels_.clear();
}

bool IceAgent::create_channel(EventLoop* el, const std::string& transport_name, IceCandidateComponent component) {
    if (get_channel(transport_name, component)) {
        return true;
    }
    
    auto channel = new IceTransportChannel(el, port_allocator_, transport_name, component);
    channel->signal_candidate_allocate_done.connect(this, &IceAgent::_on_candidate_allocate_done);
    channel->signal_receiving_state.connect(this, &IceAgent::_on_ice_receiving_state);
    channel->signal_writable_state.connect(this, &IceAgent::_on_ice_writable_state);
    channel->signal_ice_state_changed.connect(this, &IceAgent::_on_ice_state_changed);
    channel->signal_read_packet.connect(this, &IceAgent::_on_read_packet);
    channels_.push_back(channel);

    return true;
}

void IceAgent::_on_candidate_allocate_done(IceTransportChannel* channel,
            const std::vector<Candidate>& candidates)
{
    signal_candidate_allocate_done(this, channel->transport_name(),
            channel->component(), candidates);
}

void IceAgent::_on_ice_receiving_state(IceTransportChannel*) {
    _update_state();
}

void IceAgent::_on_ice_writable_state(IceTransportChannel*) {
    _update_state();
}

void IceAgent::_on_ice_state_changed(IceTransportChannel*) {
    _update_state();
}

void IceAgent::_on_read_packet(IceTransportChannel* channel, const char* buf, size_t len, int64_t ts) {
    signal_read_packet(this, channel->transport_name(), channel->component(), buf, len, ts);
}

void IceAgent::_update_state() {
    IceTransportState ice_state = IceTransportState::k_new;

    std::map<IceTransportState, int> ice_state_counts;
    for (auto channel : channels_) {
        ice_state_counts[channel->state()]++;
    }

    int total_ice_new = ice_state_counts[IceTransportState::k_new];
    int total_ice_checking = ice_state_counts[IceTransportState::k_checking];
    int total_ice_connected = ice_state_counts[IceTransportState::k_connected];
    int total_ice_completed = ice_state_counts[IceTransportState::k_completed];
    int total_ice_failed = ice_state_counts[IceTransportState::k_failed];
    int total_ice_disconnected = ice_state_counts[IceTransportState::k_disconnected];
    int total_ice_closed = ice_state_counts[IceTransportState::k_closed];
    int total_ice = channels_.size();

    if (total_ice_failed > 0) {
        ice_state = IceTransportState::k_failed;
    } else if (total_ice_disconnected > 0) {
        ice_state = IceTransportState::k_disconnected;
    } else if (total_ice_new + total_ice_closed == total_ice) {
        ice_state = IceTransportState::k_new;
    } else if (total_ice_new + total_ice_checking > 0) {
        ice_state = IceTransportState::k_checking;
    } else if (total_ice_completed + total_ice_closed == total_ice) {
        ice_state = IceTransportState::k_completed;
    } else if (total_ice_connected + total_ice_completed + total_ice_closed == total_ice) {
        ice_state = IceTransportState::k_connected;
    }

    if (ice_state_ != ice_state) {
        // 为了保证不跳过k_connected状态
        if (ice_state_ == IceTransportState::k_checking && ice_state == IceTransportState::k_completed) {
            signal_ice_state(this, IceTransportState::k_connected);
        }

        ice_state_ = ice_state;
        signal_ice_state(this, ice_state_);
    } 
}

IceTransportChannel* IceAgent::get_channel(const std::string& transport_name,
        IceCandidateComponent component) {
    auto iter = _get_channels(transport_name, component);
    return iter == channels_.end() ? nullptr : *iter;
}

std::vector<IceTransportChannel*>::iterator IceAgent::_get_channels(
        const std::string& transport_name,
        IceCandidateComponent component) {
    return std::find_if(channels_.begin(), channels_.end(), 
                [transport_name, component](IceTransportChannel *channel) {
        return transport_name == channel->transport_name() && component == channel->component();
    });
}

void IceAgent::set_ice_params(const std::string& transport_name,
        IceCandidateComponent component,
        const IceParameters& ice_params)
{
    auto channel = get_channel(transport_name, component);
    if (channel) {
        channel->set_ice_params(ice_params);
    }
}

void IceAgent::set_remote_ice_params(const std::string& transport_name,
        IceCandidateComponent component,
        const IceParameters& ice_params)
{
    auto channel = get_channel(transport_name, component);
    if (channel) {
        channel->set_remote_ice_params(ice_params);
    }
}

void IceAgent::gathering_candidate() {
    for (auto channel : channels_) {
        channel->gathering_candidate();
    }     
}

int IceAgent::send_unencrypted_rtcp(const std::string& transport_name, IceCandidateComponent component, const char* buf, size_t size) {
    IceTransportChannel* channel = get_channel(transport_name, component);
    if (channel) {
        return channel->send_packet(buf, size);
    }

    return -1;
}

} // namespace xrtc