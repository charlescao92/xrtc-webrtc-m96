/**
 * @file codec_info.h
 * @author charles
 * @brief 
*/

#ifndef  __DTLS_TRANSPORT_H_
#define  __DTLS_TRANSPORT_H_

#include <memory>

#include <rtc_base/third_party/sigslot/sigslot.h>
#include <rtc_base/ssl_stream_adapter.h>
#include <rtc_base/buffer_queue.h>
#include <rtc_base/rtc_certificate.h>

#include "ice/ice_transport_channel.h"

namespace xrtc {

enum class DtlsTransportState {
    k_new,
    k_connecting,
    k_connected,
    k_closed,
    k_failed,
    k_num_values
};

class StreamInterfaceChannel : public rtc::StreamInterface {
public:
    StreamInterfaceChannel(IceTransportChannel *ice_channel);

    bool on_received_packet(const char* data, size_t size);

    rtc::StreamState GetState() const override;
    rtc::StreamResult Read(void* buffer,
            size_t buffer_len,
            size_t* read,
            int* error) override;
    rtc::StreamResult Write(const void* data,
            size_t data_len,
            size_t* written,
            int* error) override;
    void Close() override;

private:
    IceTransportChannel *ice_channel_ = nullptr;
    rtc::BufferQueue packets_;
    rtc::StreamState state_ = rtc::SS_OPEN; 
};

class DtlsTransport : public sigslot::has_slots<> {
public:
    DtlsTransport(IceTransportChannel* ice_channel);
    ~DtlsTransport();
    
    const std::string& transport_name() { return ice_channel_->transport_name(); }
    IceCandidateComponent component() { return ice_channel_->component(); }
    DtlsTransportState dtls_state() const { return dtls_state_; }
    IceTransportChannel* ice_channel() { return ice_channel_; }
    bool is_dtls_active() { return dtls_active_; }
    bool writable() { return writable_; }

    int send_packet(const char* data, size_t len);

    bool set_local_certificate(rtc::RTCCertificate* cert);
    bool set_remote_fingerprint(const std::string& digest_alg, const uint8_t* digest, size_t digest_len);

    std::string to_string();
    bool get_srtp_crypto_suite(int* selected_crypto_suite);
    bool export_keying_material(const std::string& label,
            const uint8_t* context, 
            size_t context_len,
            bool use_context,
            uint8_t* result,
            size_t result_len);

    sigslot::signal2<DtlsTransport*, DtlsTransportState> signal_dtls_state;
    sigslot::signal1<DtlsTransport*> signal_writable_state;
    sigslot::signal1<DtlsTransport*> signal_receiving_state;
    sigslot::signal4<DtlsTransport*, const char*, size_t, int64_t> signal_read_packet;
    sigslot::signal1<DtlsTransport*> signal_closed;

private:
    void _on_read_packet(IceTransportChannel* channel, const char* buf, size_t len, int64_t ts);
    bool _setup_dtls();
    void _maybe_start_dtls();
    void _set_dtls_state(DtlsTransportState state);
    void _set_writable_state(bool writable);
    void _set_receiving(bool receiving);
    bool _handle_dtls_packet(const char* data, size_t size);
    void _on_writable_state(IceTransportChannel* channel);
    void _on_receiving_state(IceTransportChannel* channel);
    void _on_dtls_event(rtc::StreamInterface* dtls, int sig, int error);
    void _on_dtls_handshake_error(rtc::SSLHandshakeError error);

private:
    IceTransportChannel *ice_channel_ = nullptr;
    DtlsTransportState dtls_state_ = DtlsTransportState::k_new;
    bool receiving_ = false;
    bool writable_ = false;
    std::unique_ptr<rtc::SSLStreamAdapter> dtls_;
    rtc::Buffer cached_client_hello_;
    rtc::RTCCertificate *local_certificate_ = nullptr;
    StreamInterfaceChannel *downward_ = nullptr;
    rtc::Buffer remote_fingerprint_value_;
    std::string remote_fingerprint_alg_;
    bool dtls_active_ = false;
    std::vector<int> srtp_ciphers_;
};

} // namespace xrtc

#endif  //__DTLS_TRANSPORT_H_
