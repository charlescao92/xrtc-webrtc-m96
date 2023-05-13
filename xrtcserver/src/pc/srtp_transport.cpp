#include <rtc_base/logging.h>

#include "pc/srtp_transport.h"

namespace xrtc {

SrtpTransport::SrtpTransport(bool rtcp_mux_enabled) : 
    rtcp_mux_enabled_(rtcp_mux_enabled) {}


bool SrtpTransport::set_rtp_params(int send_cs,
        const uint8_t* send_key,
        size_t send_key_len,
        const std::vector<int>& send_extension_ids,
        int recv_cs,
        const uint8_t* recv_key,
        size_t recv_key_len,
        const std::vector<int>& recv_extension_ids)
{

    bool new_session = false;
    if (!send_session_) {
        _create_srtp_session();
        new_session = true;
    }

    bool ret = new_session 
        ? send_session_->set_send(send_cs, send_key, send_key_len, send_extension_ids)
        : send_session_->update_send(send_cs, send_key, send_key_len, send_extension_ids);
    if (!ret) {
        reset_params();
        return false;
    }

    ret = new_session 
        ? recv_session_->set_recv(recv_cs, recv_key, recv_key_len, recv_extension_ids)
        : recv_session_->update_recv(recv_cs, recv_key, recv_key_len, recv_extension_ids);
    if (!ret) {
        reset_params();
        return false;
    }

    RTC_LOG(LS_INFO) << "SRTP [" << (new_session ? "activated" : "updated")
        << "] params: send crypto suite: " << send_cs 
        << " , recv crypto suite : " << recv_cs;

    return true;   
}

void SrtpTransport::reset_params() {
    send_session_ = nullptr;
    recv_session_ = nullptr;
    RTC_LOG(LS_INFO) << "The params in SRTP reset";
}

void SrtpTransport::_create_srtp_session() {
    send_session_.reset(new SrtpSession());
    recv_session_.reset(new SrtpSession());
}

bool SrtpTransport::is_srtp_active() {
    return send_session_ && recv_session_;
}

bool SrtpTransport::unprotect_rtp(void* p, int in_len, int* out_len) {
    if (!is_srtp_active()) {
        return false;
    }
    return recv_session_->unprotect_rtp(p, in_len, out_len);
}   

bool SrtpTransport::unprotect_rtcp(void* p, int in_len, int* out_len) {
    if (!is_srtp_active()) {
        return false;
    }
    return recv_session_->unprotect_rtcp(p, in_len, out_len);
} 

void SrtpTransport::get_send_auth_tag_len(int* rtp_auth_tag_len, int* rtcp_auth_tag_len) {
    if (send_session_) {
        send_session_->get_auth_tag_len(rtp_auth_tag_len, rtcp_auth_tag_len);
    }
}

bool SrtpTransport::protect_rtp(void* p, int in_len, int max_len, int* out_len) {
    if (!is_srtp_active()) {
        return false;
    }
    return send_session_->protect_rtp(p, in_len, max_len, out_len);
}

bool SrtpTransport::protect_rtcp(void* p, int in_len, int max_len, int* out_len) {
    if (!is_srtp_active()) {
        return false;
    }
    return send_session_->protect_rtcp(p, in_len, max_len, out_len);
}

} // end namespace xrtc
