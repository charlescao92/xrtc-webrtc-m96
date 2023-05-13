/**
 * @file ice_agent.h
 * @author charles
 * @brief 
*/

#ifndef __ICE_AGENT_H_
#define __ICE_AGENT_H_

#include <vector>

#include <rtc_base/third_party/sigslot/sigslot.h>

#include "ice/ice_def.h"
#include "ice/ice_credentials.h"
#include "ice/ice_transport_channel.h"

namespace xrtc {

class EventLoop;
class PortAllocator;
class Candidate;

class IceAgent : public sigslot::has_slots<> {
public:
    IceAgent(EventLoop *el, PortAllocator *allocator);
    ~IceAgent();

    // transport_name: audio 或者 video
    // component : rtp或者rtcp
    bool create_channel(EventLoop* el, const std::string& transport_name, IceCandidateComponent component);
    IceTransportChannel* get_channel(const std::string& transport_name, IceCandidateComponent component);

    int send_unencrypted_rtcp(const std::string& transport_name, IceCandidateComponent component, 
                            const char* buf, size_t size);
        
    void set_ice_params(const std::string& transport_name, 
                IceCandidateComponent component, 
                const IceParameters& ice_params);
    void set_remote_ice_params(const std::string& transport_name,
            IceCandidateComponent component,
            const IceParameters& ice_params);

    void gathering_candidate();

    IceTransportState ice_state() { return ice_state_; }

    sigslot::signal4<IceAgent*, const std::string&, IceCandidateComponent,
        const std::vector<Candidate>&> signal_candidate_allocate_done;
    sigslot::signal2<IceAgent*, IceTransportState> signal_ice_state;
    sigslot::signal6<IceAgent*, const std::string&, int, const char*, size_t, int64_t> signal_read_packet;

private:
    std::vector<IceTransportChannel*>::iterator _get_channels(
            const std::string& transport_name,
            IceCandidateComponent component);  
    void _on_candidate_allocate_done(IceTransportChannel* channel, const std::vector<Candidate>& candidates); 
    void _on_ice_receiving_state(IceTransportChannel* channel);
    void _on_ice_writable_state(IceTransportChannel* channel);
    void _on_ice_state_changed(IceTransportChannel* channel);
    void _on_read_packet(IceTransportChannel*, const char* buf, size_t len, int64_t ts);
    void _update_state();

private:
    EventLoop *el_ = nullptr;
    PortAllocator *port_allocator_ = nullptr;
    std::vector<IceTransportChannel*> channels_;
    IceTransportState ice_state_ = IceTransportState::k_new;

};

} // end namespace xrtc

#endif // __ICE_AGENT_H_
