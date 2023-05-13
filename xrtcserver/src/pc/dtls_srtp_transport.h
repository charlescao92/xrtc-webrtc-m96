/**
 * @file dtls_srtp_transport.h
 * @author charles
 * @brief 
*/

#ifndef  __DTLS_SRTP_TRANSPORT_H_
#define  __DTLS_SRTP_TRANSPORT_H_

#include <string>

#include <rtc_base/buffer.h>
#include <rtc_base/copy_on_write_buffer.h>

#include "pc/srtp_transport.h"

namespace xrtc {

class DtlsTransport;

class DtlsSrtpTransport : public SrtpTransport {
public:
    DtlsSrtpTransport(const std::string& transport_name, bool rtcp_mux_enabled);
    
public:
    void set_dtls_transports(DtlsTransport *rtp_dtls_transport, DtlsTransport *rtcp_dtls_transport);
    bool is_dtls_writable();
    const std::string& transport_name() { return transport_name_; }
    int send_rtp(const char* buf, size_t size);
    int send_rtcp(const char* buf, size_t size);

    sigslot::signal3<DtlsSrtpTransport*, rtc::CopyOnWriteBuffer*, int64_t> signal_rtp_packet_received;
    sigslot::signal3<DtlsSrtpTransport*, rtc::CopyOnWriteBuffer*, int64_t> signal_rtcp_packet_received;

private:
    bool _extract_params(DtlsTransport* dtls_transport,
            int* selected_crypto_suite,
            rtc::ZeroOnFreeBuffer<unsigned char>* send_key,
            rtc::ZeroOnFreeBuffer<unsigned char>* recv_key);
    void _maybe_setup_dtls_srtp();
    void _setup_dtls_srtp();
    void _on_dtls_state(DtlsTransport* dtls, DtlsTransportState state);
    void _on_read_packet(DtlsTransport* dtls, const char* data, size_t len, int64_t ts);
    void _on_rtp_packet_received(rtc::CopyOnWriteBuffer packet, int64_t ts);
    void _on_rtcp_packet_received(rtc::CopyOnWriteBuffer packet, int64_t ts);

private:
    std::string transport_name_;
    DtlsTransport *rtp_dtls_transport_ = nullptr;
    DtlsTransport *rtcp_dtls_transport_ = nullptr;
    int unprotect_fail_count_ = 0;
    uint16_t last_send_seq_num_ = 0;
};

} // end namespace xrtc

#endif  //__DTLS_SRTP_TRANSPORT_H_
