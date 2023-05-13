#include <rtc_base/logging.h>
#include <api/crypto/crypto_options.h>

#include "pc/dtls_transport.h"

namespace xrtc {

const size_t k_dtls_record_header_len = 13;
const size_t k_max_dtls_packet_len = 2048;
const size_t k_max_pending_packets = 2;
const size_t k_min_rtp_packet_len = 12;

bool is_dtls_packet(const char* buf, size_t len) {
    const uint8_t* u = reinterpret_cast<const uint8_t*>(buf);
    return len >= k_dtls_record_header_len && (u[0] > 19 && u[0] < 64);
}

bool is_dtls_client_hello_packet(const char* buf, size_t len) {
    if (!is_dtls_packet(buf, len)) {
        return false;
    }

    const uint8_t* u = reinterpret_cast<const uint8_t*>(buf);
    return len > 17 && (u[0] == 22 && u[13] == 1);
}

bool is_rtp_packet(const char* buf, size_t len) {
    const uint8_t* u = reinterpret_cast<const uint8_t*>(buf);
    return len >= k_min_rtp_packet_len && ((u[0] & 0xC0) == 0x80);
}

StreamInterfaceChannel::StreamInterfaceChannel(IceTransportChannel* ice_channel) :
    ice_channel_(ice_channel),
    packets_(k_max_pending_packets, k_max_dtls_packet_len)
{
}

bool StreamInterfaceChannel::on_received_packet(const char* data, size_t size) {
    if (packets_.size() > 0) {
        RTC_LOG(LS_INFO) << ": Packet already in buffer queue";
    }

    if (!packets_.WriteBack(data, size, NULL)) {
        RTC_LOG(LS_WARNING) << ": Failed to write packet to queue";
    }

    SignalEvent(this, rtc::SE_READ, 0);

    return true;
}

rtc::StreamState StreamInterfaceChannel::GetState() const {
    return state_;
}

rtc::StreamResult StreamInterfaceChannel::Read(void* buffer,
        size_t buffer_len,
        size_t* read,
        int* /*error*/)
{
    if (state_ == rtc::SS_CLOSED) {
        return rtc::SR_EOS;
    }

    if (state_ == rtc::SS_OPENING) {
        return rtc::SR_BLOCK;
    }

    if (!packets_.ReadFront(buffer, buffer_len, read)) {
        return rtc::SR_BLOCK;
    }

    return rtc::SR_SUCCESS;  
}

rtc::StreamResult StreamInterfaceChannel::Write(const void* data,
        size_t data_len,
        size_t* written,
        int* /*error*/) 
{
    ice_channel_->send_packet((const char*)data, data_len);
    if (written) {
        *written = data_len;
    }

    return rtc::SR_SUCCESS; 
}

void StreamInterfaceChannel::Close() {
    packets_.Clear();
    state_ = rtc::SS_CLOSED;
}

DtlsTransport::DtlsTransport(IceTransportChannel* ice_channel) :
    ice_channel_(ice_channel)
{
    ice_channel_->signal_read_packet.connect(this, &DtlsTransport::_on_read_packet);
    ice_channel_->signal_writable_state.connect(this, &DtlsTransport::_on_writable_state);
    ice_channel_->signal_receiving_state.connect(this, &DtlsTransport::_on_receiving_state);

    webrtc::CryptoOptions crypto_options;
    srtp_ciphers_ = crypto_options.GetSupportedDtlsSrtpCryptoSuites();
}

DtlsTransport::~DtlsTransport() {
}

void DtlsTransport::_on_read_packet(IceTransportChannel* /*channel*/,
        const char* buf, size_t len, int64_t ts)
{
    switch (dtls_state_) {
        case DtlsTransportState::k_new:
            if (dtls_) {
                RTC_LOG(LS_INFO) << to_string() << ": Received packet before DTLS started.";
            } else {
                RTC_LOG(LS_WARNING) << to_string() << ": Received packet before we know if "
                    << "we are doing DTLS or not";
            }
               
            if (is_dtls_client_hello_packet(buf, len)) {
                RTC_LOG(LS_INFO) << to_string() << ": Caching DTLS ClientHello packet until "
                    << "DTLS started";
                cached_client_hello_.SetData(buf, len);

                if (!dtls_ && local_certificate_) {
                    _setup_dtls();
                }

            } else {
                RTC_LOG(LS_WARNING) << to_string() << ": Not a DTLS ClientHello packet, "
                    << "dropping";
            }
            break;
        case DtlsTransportState::k_connecting:
        case DtlsTransportState::k_connected:
            if (is_dtls_packet(buf, len)) { // Dtls包
                if (!_handle_dtls_packet(buf, len)) {
                    RTC_LOG(LS_WARNING) << to_string() << ": handle DTLS packet failed";
                    return;
                }
            } else { // RTP/RTCP包 
                if (dtls_state_ != DtlsTransportState::k_connected) {
                    RTC_LOG(LS_WARNING) << to_string() << ": Received non DTLS packet "
                        << "before DTLS complete";
                    return;
                }

                if (!is_rtp_packet(buf, len)) {
                    RTC_LOG(LS_WARNING) << to_string() << ": Received unexpected non "
                        << "DTLS packet";
                    return;
                }

               // RTC_LOG(LS_INFO) << "==============rtp received: " << len;
                signal_read_packet(this, buf, len, ts);
            }
            break;
        default:
            break;
  }    
}

bool DtlsTransport::set_local_certificate(rtc::RTCCertificate* cert) {
   if (dtls_active_) {
        if (cert == local_certificate_) {
            RTC_LOG(LS_INFO) << to_string() << ": Ingnoring identical DTLS cert";
            return true;
        } else {
            RTC_LOG(LS_WARNING) << to_string() << ": Cannot change cert in this state";
            return false;
        }
    }

    if (cert) {
        local_certificate_ = cert;
        dtls_active_ = true;
    }

    return true;   
}

bool DtlsTransport::set_remote_fingerprint(const std::string& digest_alg,
        const uint8_t* digest, size_t digest_len) 
{
    rtc::Buffer remote_fingerprint_value(digest, digest_len);

    if (dtls_active_ && remote_fingerprint_value_ == remote_fingerprint_value && 
            !digest_alg.empty()) 
    {
        RTC_LOG(LS_INFO) << to_string() << ": Ignoring identical remote fingerprint";
        return true;
    }

    if (digest_alg.empty()) {
        RTC_LOG(LS_WARNING) << to_string() << ": Other sides not support DTLS";
        dtls_active_ = false;
        return false;
    }

    if (!dtls_active_) {
        RTC_LOG(LS_WARNING) << to_string() << ": Cannot set remote fingerpint in this state";
        return false;
    }

    bool fingerprint_change = remote_fingerprint_value_.size() > 0u;

    remote_fingerprint_value_ = std::move(remote_fingerprint_value);
    remote_fingerprint_alg_ = digest_alg;

    // ClientHello packet先到，answer sdp后到
    if (dtls_ && !fingerprint_change) {
        rtc::SSLPeerCertificateDigestError err;
        if (!dtls_->SetPeerCertificateDigest(digest_alg, (const unsigned char*)digest, digest_len, &err)) {
            RTC_LOG(LS_WARNING) << to_string() << ": Failed to set peer certificate digest";
            _set_dtls_state(DtlsTransportState::k_failed);
            return err == rtc::SSLPeerCertificateDigestError::VERIFICATION_FAILED;
        }

        return true;
    }

    if (dtls_ && fingerprint_change) {
        dtls_.reset(nullptr);
        _set_dtls_state(DtlsTransportState::k_new);
        _set_writable_state(false);
    }

    if (!_setup_dtls()) {
        RTC_LOG(LS_WARNING) << to_string() << ": Failed to setup DTLS";
        _set_dtls_state(DtlsTransportState::k_failed);
        return false;
    }
    
    return true; 
}

void DtlsTransport::_set_dtls_state(DtlsTransportState state) {
    if (dtls_state_ == state) {
        return;
    }

    RTC_LOG(LS_INFO) << to_string() << ": Change dtls state from " << dtls_state_
        << " to " << state;

    dtls_state_ = state;
    signal_dtls_state(this, state);
}

void DtlsTransport::_set_writable_state(bool writable) {
    if (writable_ == writable) {
        return;
    }

    RTC_LOG(LS_INFO) << to_string() << ": set DTLS writable to " << writable;
    writable_ = writable;
    signal_writable_state(this);
}

bool DtlsTransport::_setup_dtls() {
    auto downward = std::make_unique<StreamInterfaceChannel>(ice_channel_); 
    StreamInterfaceChannel* downward_ptr = downward.get();
    
    dtls_ = rtc::SSLStreamAdapter::Create(std::move(downward));
    if (!dtls_) {
        RTC_LOG(LS_WARNING) << to_string() << ": Failed to create SSLStreamAdapter";
        return false;
    }

    downward_ = downward_ptr;

    dtls_->SetIdentity(local_certificate_->identity()->Clone());
    dtls_->SetMode(rtc::SSL_MODE_DTLS);
    dtls_->SetMaxProtocolVersion(rtc::SSL_PROTOCOL_DTLS_12);
    dtls_->SetServerRole(rtc::SSL_SERVER);
    dtls_->SignalEvent.connect(this, &DtlsTransport::_on_dtls_event);
    dtls_->SignalSSLHandshakeError.connect(this, &DtlsTransport::_on_dtls_handshake_error);
    
    if (remote_fingerprint_value_.size() && !dtls_->SetPeerCertificateDigest(
            remote_fingerprint_alg_,
            remote_fingerprint_value_.data(),
            remote_fingerprint_value_.size()))
    {
        RTC_LOG(LS_WARNING) << to_string() << ": Failed to set remote fingerprint";
        return false;
    }

    if (!srtp_ciphers_.empty()) {
        if (!dtls_->SetDtlsSrtpCryptoSuites(srtp_ciphers_)) {
            RTC_LOG(LS_WARNING) << to_string() << ": Failed to set DTLS-SRTP crypto suites";
            return false;
        }
    } else {
        RTC_LOG(LS_WARNING) << to_string() << ": Not using DTLS-SRTP";
    }
    
    RTC_LOG(LS_INFO) << to_string() << ": Setup DTLS complete";
    
    _maybe_start_dtls();

    return true;
}

void DtlsTransport::_on_dtls_event(rtc::StreamInterface* /*dtls*/, int sig, int error) {
    if (sig & rtc::SE_OPEN) {
        RTC_LOG(LS_INFO) << to_string() << ": DTLS handshake complete.";
        _set_writable_state(true);
        _set_dtls_state(DtlsTransportState::k_connected);
    }

    if (sig & rtc::SE_READ) {
        char buf[k_max_dtls_packet_len];
        size_t read;
        int read_error;
        rtc::StreamResult ret;
        // 因为一个数据包可能会包含多个DTLS record，需要循环读取
        do {
            ret = dtls_->Read(buf, sizeof(buf), &read, &read_error);
            if (ret == rtc::SR_SUCCESS) {
            } else if (ret == rtc::SR_EOS) {
                RTC_LOG(LS_INFO) << to_string() << ": DTLS transport closed by remote.";
                _set_writable_state(false);
                _set_dtls_state(DtlsTransportState::k_closed);
                signal_closed(this);
            } else if (ret == rtc::SR_ERROR) {
                RTC_LOG(LS_WARNING) << to_string() << ": Closed DTLS transport by remote with error, code=" << read_error;
                _set_writable_state(false);
                _set_dtls_state(DtlsTransportState::k_failed);
                signal_closed(this);
            }
        } while (ret == rtc::SR_SUCCESS);
    }

    if (sig & rtc::SE_CLOSE) {
        if (!error) {
            RTC_LOG(LS_INFO) << to_string() << ": DTLS transport closed";
            _set_writable_state(false);
            _set_dtls_state(DtlsTransportState::k_closed);
        } else {
            RTC_LOG(LS_INFO) << to_string() << ": DTLS transport closed with error code=" << error;
            _set_writable_state(false);
            _set_dtls_state(DtlsTransportState::k_failed);
        }
    }
}

void DtlsTransport::_on_dtls_handshake_error(rtc::SSLHandshakeError err) {
    RTC_LOG(LS_WARNING) << to_string() << ": DTLS handshake error=" << (int)err;
}

void DtlsTransport::_maybe_start_dtls() {
    if (dtls_ && ice_channel_->writable()) {
        if (dtls_->StartSSL()) {
            RTC_LOG(LS_WARNING) << to_string() << ": Failed to StartSSL.";
            _set_dtls_state(DtlsTransportState::k_failed);
            return;
        }

        RTC_LOG(LS_INFO) << to_string() << ": Started DTLS.";
        _set_dtls_state(DtlsTransportState::k_connecting);

        if (cached_client_hello_.size() > 0) {
            if (!_handle_dtls_packet(cached_client_hello_.data<char>(), cached_client_hello_.size())) {
                RTC_LOG(LS_WARNING) << to_string() << ": Handling dtls packet failed.";
                _set_dtls_state(DtlsTransportState::k_failed);
            }
            cached_client_hello_.Clear();
        }
    }
}

std::string DtlsTransport::to_string() {
    std::stringstream ss;
    absl::string_view RECEIVING[2] = {"-", "R"};
    absl::string_view WRITABLE[2] = {"-", "W"};

    ss << "DtlsTransport[" << transport_name() << "|"
        << (int)component() << "|"
        << RECEIVING[receiving_] << "|"
        << WRITABLE[writable_] << "]";
    return ss.str();
}

bool DtlsTransport::_handle_dtls_packet(const char* data, size_t size) {
    const uint8_t* tmp_data = reinterpret_cast<const uint8_t*>(data);
    size_t tmp_size = size;

    while (tmp_size > 0) {
        if (tmp_size < k_dtls_record_header_len) {
            return false;
        }

        // body部分的长度record_len，保存在头部的最后两个字节
        size_t record_len = (tmp_data[11] << 8) | tmp_data[12];
        if (record_len + k_dtls_record_header_len > tmp_size) {
            return false;
        }

        tmp_data += k_dtls_record_header_len + record_len;
        tmp_size -= k_dtls_record_header_len + record_len;
    }

    return downward_->on_received_packet(data, size);
}

void DtlsTransport::_on_writable_state(IceTransportChannel* channel) {
    RTC_LOG(LS_INFO) << to_string() << ": IceTransportChannel writable changed to " << channel->writable();

    if (!dtls_active_) {
        _set_writable_state(channel->writable());
        return;
    }

    switch (dtls_state_) {
        case DtlsTransportState::k_new:
            _maybe_start_dtls();
            break;
        case DtlsTransportState::k_connected:
            _set_writable_state(channel->writable());
            break;
        default:
            break;
    }
}

void DtlsTransport::_on_receiving_state(IceTransportChannel* channel) {
    _set_receiving(channel->receiving());
}

void DtlsTransport::_set_receiving(bool receiving) {
    if (receiving_ == receiving) {
        return;
    }

    RTC_LOG(LS_INFO) << to_string() << ": Change receiving to " << receiving;
    receiving_ = receiving;
    signal_receiving_state(this);
}

bool DtlsTransport::get_srtp_crypto_suite(int* selected_crypto_suite) {
    if (dtls_state_ != DtlsTransportState::k_connected) {
        return false;
    }

    return dtls_->GetDtlsSrtpCryptoSuite(selected_crypto_suite);
}

bool DtlsTransport::export_keying_material(const std::string& label,
        const uint8_t* context,
        size_t context_len,
        bool use_context,
        uint8_t* result,
        size_t result_len)
{
    return dtls_.get() ? dtls_->ExportKeyingMaterial(label, context, context_len,
            use_context, result, result_len) : false;
}

int DtlsTransport::send_packet(const char* data, size_t len) {
    if (ice_channel_) {
        return ice_channel_->send_packet(data, len);
    }

    return -1;
}

} // namespace xrtc

