
/**
 * @file transport_controller.h
 * @author charles
 * @brief 
*/

#ifndef  __TRANSPORT_CONTROLLER_H_
#define  __TRANSPORT_CONTROLLER_H_

#include <vector>
#include <map>

#include <rtc_base/third_party/sigslot/sigslot.h>
#include <rtc_base/copy_on_write_buffer.h>

#include "ice/ice_def.h"
#include "pc/peer_connection_def.h"
#include "ice/ice_transport_channel.h"

namespace xrtc {

class EventLoop;
class IceAgent;
class SessionDescription;
class PortAllocator;
class Candidate;
class DtlsTransport;
enum class DtlsTransportState;
class DtlsSrtpTransport;

class TransportController : public sigslot::has_slots<> {
public:
    TransportController(EventLoop *el, PortAllocator *allocator, bool dtls_on);
    ~TransportController();
    
    int set_local_description(SessionDescription* desc);
    int set_remote_description(SessionDescription* desc);
    void set_local_certificate(rtc::RTCCertificate* cert);

    int send_rtp(const std::string& transport_name, const char* data, size_t len);
    int send_rtcp(const std::string& transport_name, const char* data, size_t len);
    int send_unencrypted_rtcp(const std::string& transport_name, const char* data, size_t len);
    
    sigslot::signal4<TransportController*, const std::string&, IceCandidateComponent,
        const std::vector<Candidate>&> signal_candidate_allocate_done;
    sigslot::signal2<TransportController*, PeerConnectionState> signal_connection_state;
    sigslot::signal3<TransportController*, rtc::CopyOnWriteBuffer*, int64_t> signal_rtp_packet_received;
    sigslot::signal3<TransportController*, rtc::CopyOnWriteBuffer*, int64_t> signal_rtcp_packet_received;

private:
    void _on_candidate_allocate_done(IceAgent* agent,
            const std::string& transport_name,
            IceCandidateComponent component,
            const std::vector<Candidate>& candidates);
    void _on_dtls_receiving_state(DtlsTransport*);
    void _on_dtls_writable_state(DtlsTransport*);
    void _on_dtls_state(DtlsTransport*, DtlsTransportState);
    void _on_ice_state(IceAgent*, IceTransportState);
    void _on_rtp_packet_received(DtlsSrtpTransport*, rtc::CopyOnWriteBuffer* packet, int64_t ts);
    void _on_rtcp_packet_received(DtlsSrtpTransport*, rtc::CopyOnWriteBuffer* packet, int64_t ts);

    void _add_dtls_transport(DtlsTransport* dtls);
    DtlsTransport* _get_dtls_transport(const std::string& transport_name);
    void _add_dtls_srtp_transport(DtlsSrtpTransport* dtls);
    DtlsSrtpTransport* _get_dtls_srtp_transport(const std::string& transport_name);
    void _update_state();
    void _on_read_packet(IceAgent*, const std::string&, int, const char* data, size_t len, int64_t ts);

private:
    EventLoop *el_ = nullptr;
    IceAgent *ice_agent_ = nullptr;
    std::map<std::string, DtlsTransport*> dtls_transport_by_name_;
    std::map<std::string, DtlsSrtpTransport*> dtls_srtp_transport_by_name_;
    rtc::RTCCertificate *local_certificate_ = nullptr;
    PeerConnectionState pc_state_ = PeerConnectionState::k_new;
    bool dtls_on_ = true;
};

} // end namespace xrtc

#endif  // __TRANSPORT_CONTROLLER_H_


