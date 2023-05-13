#include <rtc_base/logging.h>

#include "ice/ice_agent.h"
#include "ice/ice_credentials.h"
#include "ice/candidate.h"
#include "pc/session_description.h"
#include "pc/transport_controller.h"
#include "pc/dtls_transport.h"
#include "pc/dtls_srtp_transport.h"
#include "modules/rtp_rtcp/rtp_utils.h"

namespace xrtc {

TransportController::TransportController(EventLoop *el, PortAllocator *allocator, bool dtls_on) :
    el_(el),
    ice_agent_(new IceAgent(el, allocator)),
    dtls_on_(dtls_on)
{
    ice_agent_->signal_candidate_allocate_done.connect(this,
        &TransportController::_on_candidate_allocate_done);
}

TransportController::~TransportController() {
    for (auto dtls_srtp : dtls_srtp_transport_by_name_) {
        delete dtls_srtp.second;
    }
    dtls_srtp_transport_by_name_.clear();

    for (auto dtls : dtls_transport_by_name_) {
        delete dtls.second;
    }
    dtls_transport_by_name_.clear();

    if (ice_agent_) {
        delete ice_agent_;
        ice_agent_ = nullptr;
    }
}

void TransportController::_on_candidate_allocate_done(IceAgent* /*agent*/,
        const std::string& transport_name,
        IceCandidateComponent component,
        const std::vector<Candidate>& candidates)
{
    signal_candidate_allocate_done(this, transport_name, component, candidates);
}

int TransportController::set_local_description(SessionDescription* desc) {
    if (!desc) {
        RTC_LOG(LS_WARNING) << "desc is null";
        return -1;
    }
    
    for (auto content : desc->contents()) {
        std::string mid = content->mid();
        // a=group:BUNDLE audio video
        // 如果mid在bundle中，又不是第一个成员，就不需要创建通道，只有第一个成员才创建通道
        if (desc->is_bundle(mid) && mid != desc->get_first_bundle_mid()) {
            continue;
        }

        ice_agent_->create_channel(el_, mid, IceCandidateComponent::RTP);
        std::shared_ptr<TransportDescription> td = desc->get_transport_info(mid);
        if (td) {
            ice_agent_->set_ice_params(mid, IceCandidateComponent::RTP, IceParameters(td->ice_ufrag, td->ice_pwd));
        }

        if (dtls_on_) {
            DtlsTransport* dtls = new DtlsTransport(ice_agent_->get_channel(mid, IceCandidateComponent::RTP));
            dtls->set_local_certificate(local_certificate_);
            dtls->signal_receiving_state.connect(this, &TransportController::_on_dtls_receiving_state);
            dtls->signal_receiving_state.connect(this, &TransportController::_on_dtls_writable_state);
            dtls->signal_dtls_state.connect(this, &TransportController::_on_dtls_state);
            ice_agent_->signal_ice_state.connect(this, &TransportController::_on_ice_state);
            _add_dtls_transport(dtls);

            DtlsSrtpTransport* dtls_srtp = new DtlsSrtpTransport(dtls->transport_name(), true);
            dtls_srtp->set_dtls_transports(dtls, nullptr);
                    dtls_srtp->signal_rtp_packet_received.connect(this,
                    &TransportController::_on_rtp_packet_received);
            dtls_srtp->signal_rtcp_packet_received.connect(this,
                    &TransportController::_on_rtcp_packet_received);
            _add_dtls_srtp_transport(dtls_srtp);
        } else {
            ice_agent_->signal_ice_state.connect(this, &TransportController::_on_ice_state);
            ice_agent_->signal_read_packet.connect(this, &TransportController::_on_read_packet);
        }
        
    }
    
    ice_agent_->gathering_candidate();

    return 0;
}

void TransportController::_on_read_packet(IceAgent*, const std::string& /*transport_name*/, int /*component*/, 
    const char* data, size_t len, int64_t ts) 
{

    auto array_view = rtc::MakeArrayView(data, len);

    // 推导array_view的packet类型
    RtpPacketType packet_type = infer_rtp_packet_type(array_view);
    if (packet_type == RtpPacketType::k_unknown) {
        return;
    }

    rtc::CopyOnWriteBuffer packet(data, len);
    if (packet_type == RtpPacketType::k_rtp) {
        signal_rtp_packet_received(this, &packet, ts);
    } else {
        signal_rtcp_packet_received(this, &packet, ts);
    }

}

void TransportController::_on_rtp_packet_received(DtlsSrtpTransport*,
        rtc::CopyOnWriteBuffer* packet, int64_t ts)
{
    signal_rtp_packet_received(this, packet, ts);
}

void TransportController::_on_rtcp_packet_received(DtlsSrtpTransport*,
        rtc::CopyOnWriteBuffer* packet, int64_t ts)
{
    signal_rtcp_packet_received(this, packet, ts);
}

void TransportController::_on_ice_state(IceAgent*, IceTransportState state) {
    if (dtls_on_) {
        _update_state();
    } else {
        PeerConnectionState pc_state;
        switch(state) {
            case IceTransportState::k_new:
                pc_state = PeerConnectionState::k_new;
                break;
            case IceTransportState::k_failed:
                pc_state = PeerConnectionState::k_failed;
                break;
            case IceTransportState::k_checking:
                pc_state = PeerConnectionState::k_connecting;
                break;
            case IceTransportState::k_connected:
            case IceTransportState::k_completed:
                pc_state = PeerConnectionState::k_connected;
                break;
            case IceTransportState::k_disconnected:
            case IceTransportState::k_closed:
                pc_state = PeerConnectionState::k_disconnected;
                break;
            default:
                return;
        }
        if (pc_state_ != pc_state) {
            pc_state_ = pc_state;
            signal_connection_state(this, pc_state);
        }    
    }
}

void TransportController::_on_dtls_receiving_state(DtlsTransport*) {
    _update_state();
}

void TransportController::_on_dtls_writable_state(DtlsTransport*) {
    _update_state();
}

void TransportController::_on_dtls_state(DtlsTransport*, DtlsTransportState) {
    _update_state();
}

void TransportController::_update_state() {
    PeerConnectionState pc_state = PeerConnectionState::k_new;

    std::map<DtlsTransportState, int> dtls_state_counts;
    std::map<IceTransportState, int> ice_state_counts;
    auto iter = dtls_transport_by_name_.begin();
    for (; iter != dtls_transport_by_name_.end(); ++iter) {
        dtls_state_counts[iter->second->dtls_state()]++;
        ice_state_counts[iter->second->ice_channel()->state()]++;
    }

    int total_connected = ice_state_counts[IceTransportState::k_connected] + 
        dtls_state_counts[DtlsTransportState::k_connected];
    int total_dtls_connecting = dtls_state_counts[DtlsTransportState::k_connecting];
    int total_failed = ice_state_counts[IceTransportState::k_failed] + 
        dtls_state_counts[DtlsTransportState::k_failed];
    int total_closed = ice_state_counts[IceTransportState::k_closed] + 
        dtls_state_counts[DtlsTransportState::k_closed];
    int total_new = ice_state_counts[IceTransportState::k_new] + 
        dtls_state_counts[DtlsTransportState::k_new];
    int total_ice_checking = ice_state_counts[IceTransportState::k_checking];
    int total_ice_disconnected = ice_state_counts[IceTransportState::k_disconnected];
    int total_ice_completed = ice_state_counts[IceTransportState::k_completed];
    int total_transports = dtls_transport_by_name_.size() * 2;

    if (total_failed > 0) {
        pc_state = PeerConnectionState::k_failed;
    } else if (total_ice_disconnected > 0) {
        pc_state = PeerConnectionState::k_disconnected;
    } else if (total_new + total_closed == total_transports) {
        pc_state = PeerConnectionState::k_new;
    } else if (total_ice_checking + total_dtls_connecting + total_new > 0) {
        pc_state = PeerConnectionState::k_connecting;
    } else if (total_connected + total_ice_completed + total_closed == total_transports) {
        pc_state = PeerConnectionState::k_connected;
    }

    if (pc_state_ != pc_state) {
        pc_state_ = pc_state;
        signal_connection_state(this, pc_state);
    }
}

void TransportController::_add_dtls_transport(DtlsTransport* dtls) {
    auto iter = dtls_transport_by_name_.find(dtls->transport_name());
    if (iter != dtls_transport_by_name_.end()) {
        delete iter->second;
    }

    dtls_transport_by_name_[dtls->transport_name()] = dtls;
}

DtlsTransport* TransportController::_get_dtls_transport(const std::string& transport_name) {
    auto iter = dtls_transport_by_name_.find(transport_name);
    if (iter != dtls_transport_by_name_.end()) {
        return iter->second;
    }

    return nullptr;
}

void TransportController::_add_dtls_srtp_transport(DtlsSrtpTransport* dtls_srtp) {
    auto iter = dtls_srtp_transport_by_name_.find(dtls_srtp->transport_name());
    if (iter != dtls_srtp_transport_by_name_.end()) {
        delete iter->second;
    }

    dtls_srtp_transport_by_name_[dtls_srtp->transport_name()] = dtls_srtp;
}

DtlsSrtpTransport* TransportController::_get_dtls_srtp_transport(const std::string& transport_name)  {
    auto iter = dtls_srtp_transport_by_name_.find(transport_name);
    if (iter != dtls_srtp_transport_by_name_.end()) {
        return iter->second;
    }

    return nullptr;
}

int TransportController::set_remote_description(SessionDescription* desc) {
    if (!desc) {
        return -1;
    }

    for (auto content : desc->contents()) {
        std::string mid = content->mid();
        if (desc->is_bundle(mid) && mid != desc->get_first_bundle_mid()) {
            continue;
        }
        
        auto td = desc->get_transport_info(mid);
        if (td) {
            ice_agent_->set_remote_ice_params(content->mid(), IceCandidateComponent::RTP,
                    IceParameters(td->ice_ufrag, td->ice_pwd));

            auto dtls = _get_dtls_transport(mid);
            if (dtls && td->identity_fingerprint) {
                dtls->set_remote_fingerprint(td->identity_fingerprint->algorithm,
                        td->identity_fingerprint->digest.cdata(),
                        td->identity_fingerprint->digest.size());
            }
        }
    }

    return 0;
}

void TransportController::set_local_certificate(rtc::RTCCertificate* cert) {
    local_certificate_ = cert;
}

int TransportController::send_rtp(const std::string& transport_name,  const char* data, size_t len) {
    auto dtls_srtp = _get_dtls_srtp_transport(transport_name);
    if (dtls_srtp) {
        return dtls_srtp->send_rtp(data, len);
    }
    return -1;
}

int TransportController::send_rtcp(const std::string& transport_name,  const char* data, size_t len) {
    auto dtls_srtp = _get_dtls_srtp_transport(transport_name);
    if (dtls_srtp) {
        return dtls_srtp->send_rtcp(data, len);
    }
    return -1;
}

int TransportController::send_unencrypted_rtcp(const std::string& transport_name, const char* data, size_t len) {
    if (ice_agent_) {
        // 当前RTP和RTCP共用一个通道，直接使用RTP即可
        return ice_agent_->send_unencrypted_rtcp(transport_name, IceCandidateComponent::RTP, data, len);
    }

    return -1;
}

} // end namespace xrtc
