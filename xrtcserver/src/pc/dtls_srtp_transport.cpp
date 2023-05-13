#include <rtc_base/logging.h>

#include "pc/dtls_transport.h"
#include "pc/dtls_srtp_transport.h"
#include "modules/rtp_rtcp/rtp_utils.h"

namespace xrtc {

// rfc5764
static char k_dtls_srtp_exporter_label[] = "EXTRACTOR-dtls_srtp";

DtlsSrtpTransport::DtlsSrtpTransport(const std::string& transport_name, bool rtcp_mux_enabled) :
    SrtpTransport(rtcp_mux_enabled), 
    transport_name_(transport_name)
{
}

void DtlsSrtpTransport::set_dtls_transports(DtlsTransport *rtp_dtls_transport, DtlsTransport *rtcp_dtls_transport) {
    rtp_dtls_transport_ = rtp_dtls_transport;
    rtcp_dtls_transport_ = rtcp_dtls_transport;

    if (rtp_dtls_transport_) {
        rtp_dtls_transport_->signal_dtls_state.connect(this,
                &DtlsSrtpTransport::_on_dtls_state);
        rtp_dtls_transport_->signal_read_packet.connect(this,
                &DtlsSrtpTransport::_on_read_packet);
    }

    _maybe_setup_dtls_srtp();
}

void DtlsSrtpTransport::_on_read_packet(DtlsTransport* /*dtls*/,
        const char* data, size_t len, int64_t ts)
{
    auto array_view = rtc::MakeArrayView(data, len);

    // 推导array_view的packet类型
    RtpPacketType packet_type = infer_rtp_packet_type(array_view);
    if (packet_type == RtpPacketType::k_unknown) {
        return;
    }

    rtc::CopyOnWriteBuffer packet(data, len);
    if (packet_type == RtpPacketType::k_rtcp) {
        _on_rtcp_packet_received(std::move(packet), ts);
    } else {
        _on_rtp_packet_received(std::move(packet), ts);
    }
}

void DtlsSrtpTransport::_on_rtp_packet_received(rtc::CopyOnWriteBuffer packet, int64_t ts) {
    if (!is_srtp_active()) {
        RTC_LOG(LS_WARNING) << "Inactive SRTP transport received a rtp packet, drop it.";
        return;
    }

    char *data = packet.data<char>();
    int len = packet.size();
    if (!unprotect_rtp(data, len, &len)) {
        const int k_fail_log = 100; // 失败100次才打印，控制刷屏
        if (unprotect_fail_count_ % k_fail_log == 0) {
            RTC_LOG(LS_WARNING) << "Failed to unprotect rtp packet: "
                << ", size=" << len
                << ", seqnum=" << parse_rtp_sequence_number(packet)
                << ", ssrc=" << parse_rtp_ssrc(packet)
                << ", unprotect_fail_count=" << unprotect_fail_count_;
        }
        unprotect_fail_count_++;
        return;
    }
    packet.SetSize(len);
    signal_rtp_packet_received(this, &packet, ts);
}

void DtlsSrtpTransport::_on_rtcp_packet_received(rtc::CopyOnWriteBuffer packet, int64_t ts) {
    if (!is_srtp_active()) {
        RTC_LOG(LS_WARNING) << "Inactive SRTP transport received a rtcp packet, drop it.";
        return;
    }

    char* data = packet.data<char>();
    int len = packet.size();
    if (!unprotect_rtcp(data, len, &len)) {
        int type = 0;
        get_rtcp_type(data, len, &type);
        RTC_LOG(LS_WARNING) << "Failed to unprotect rtcp packet: "
            << ", size=" << len
            << ", type=" << type;
        return;
    }
    packet.SetSize(len);
    signal_rtcp_packet_received(this, &packet, ts);
}

void DtlsSrtpTransport::_on_dtls_state(DtlsTransport* /*dtls*/, DtlsTransportState state) {
    if (state != DtlsTransportState::k_connected) {
        reset_params();
        return;
    }

    _maybe_setup_dtls_srtp();
}

bool DtlsSrtpTransport::is_dtls_writable() {
    auto rtcp_transport = rtcp_mux_enabled_ ? nullptr : rtcp_dtls_transport_;
    return rtp_dtls_transport_ && rtp_dtls_transport_->writable() &&
        (!rtcp_transport || rtcp_transport->writable());
}

void DtlsSrtpTransport::_maybe_setup_dtls_srtp() {
    if (is_srtp_active() || !is_dtls_writable()) {
        return;
    }

    _setup_dtls_srtp();
}

void DtlsSrtpTransport::_setup_dtls_srtp() {
    std::vector<int> send_extension_ids;
    std::vector<int> recv_extension_ids;

    int selected_crypto_suite;
    rtc::ZeroOnFreeBuffer<unsigned char> send_key;
    rtc::ZeroOnFreeBuffer<unsigned char> recv_key;

    // 提取成功params后直接设置
    if (!_extract_params(rtp_dtls_transport_, &selected_crypto_suite, &send_key, &recv_key) ||
        !set_rtp_params(selected_crypto_suite,
                &send_key[0], send_key.size(), send_extension_ids,
                selected_crypto_suite,
                &recv_key[0], recv_key.size(), recv_extension_ids))
    {
        RTC_LOG(LS_WARNING) << "DTLS-SRTP rtp param install failed";
    }
}

bool DtlsSrtpTransport::_extract_params(DtlsTransport* dtls_transport,
        int* selected_crypto_suite,
        rtc::ZeroOnFreeBuffer<unsigned char>* send_key,
        rtc::ZeroOnFreeBuffer<unsigned char>* recv_key)
{
    if (!dtls_transport || !dtls_transport->is_dtls_active()) {
        return false;
    }

    if (!dtls_transport->get_srtp_crypto_suite(selected_crypto_suite)) {
        RTC_LOG(LS_WARNING) << "No selected crypto suite!";
        return false;
    }

    RTC_LOG(LS_INFO) << "Extract DTLS-SRTP key from transport : " << transport_name_;

    int key_len;
    int salt_len;
    if (!rtc::GetSrtpKeyAndSaltLengths(*selected_crypto_suite, &key_len, &salt_len)) {
        RTC_LOG(LS_WARNING) << "Unknown DTLS-SRTP crypto suite: " << *selected_crypto_suite;
        return false;
    }

    //  dtls_buffer保存client和server的key和salt
    rtc::ZeroOnFreeBuffer<unsigned char> dtls_buffer(key_len * 2 + salt_len * 2);
    // false不使用上下文
    if (!dtls_transport->export_keying_material(k_dtls_srtp_exporter_label,
                NULL, 0, false, &dtls_buffer[0], dtls_buffer.size()))
    {
        RTC_LOG(LS_WARNING) << "Extracting DTLS-SRTP param failed";
        return false;
    }

    rtc::ZeroOnFreeBuffer<unsigned char> client_write_key(key_len + salt_len);
    rtc::ZeroOnFreeBuffer<unsigned char> server_write_key(key_len + salt_len);
    size_t offset = 0;
    memcpy(&client_write_key[0], &dtls_buffer[offset], key_len);
    offset += key_len;
    memcpy(&server_write_key[0], &dtls_buffer[offset], key_len);
    offset += key_len;
    memcpy(&client_write_key[key_len], &dtls_buffer[offset], salt_len);
    offset += salt_len;
    memcpy(&server_write_key[key_len], &dtls_buffer[offset], salt_len);
    
    *send_key = std::move(server_write_key);
    *recv_key = std::move(client_write_key);

    return true;
}

int DtlsSrtpTransport::send_rtp(const char* buf, size_t size) {
    if (!is_srtp_active()) {
        RTC_LOG(LS_WARNING) << "Failed to send rtp packet: Inactive srtp transport";
        return -1;
    }

    int rtp_auth_tag_len = 0;
    get_send_auth_tag_len(&rtp_auth_tag_len, nullptr);
    // size + rtp_auth_tag_len：加密后的容量
    rtc::CopyOnWriteBuffer packet(buf, size, size + rtp_auth_tag_len); 

    char* data = (char*)packet.data();
    int len = packet.size();
    uint16_t seq_num = parse_rtp_sequence_number(packet);

    // rtp加密，packet.capacity()可能会比len大
    if (!protect_rtp(data, len, packet.capacity(), &len)) {
        RTC_LOG(LS_WARNING) << "Failed to protect rtp packet, size=" << len
            << ", seqnum=" << seq_num
            << ", ssrc=" << parse_rtp_ssrc(packet)
            << ", last_send_seq_num=" << last_send_seq_num_;
        return -1;
    }
    
    last_send_seq_num_ = seq_num;

    packet.SetSize(len);
    return rtp_dtls_transport_->send_packet((const char*)packet.cdata(), packet.size());
}

int DtlsSrtpTransport::send_rtcp(const char* buf, size_t size) {
    if (!is_srtp_active()) {
        RTC_LOG(LS_WARNING) << "Failed to send rtcp packet: Inactive srtp transport";
        return -1;
    }

    int rtcp_auth_tag_len = 0;
    get_send_auth_tag_len(&rtcp_auth_tag_len, nullptr);
    // size + rtcp_auth_tag_len + sizeof(uint32_t)：加密后的容量
    rtc::CopyOnWriteBuffer packet(buf, size, size + rtcp_auth_tag_len + sizeof(uint32_t)); 

    char* data = (char*)packet.data();
    int len = packet.size();
    if (!protect_rtcp(data, len, packet.capacity(), &len)) {
        int type = 0;
        get_rtcp_type(data, len, &type);
        RTC_LOG(LS_WARNING) << "Failed to protect rtcp packet, size=" << len
            << ", type=" << type;
        return -1;
    }
    
    packet.SetSize(len);
    return rtp_dtls_transport_->send_packet((const char*)packet.cdata(), packet.size());
}

} // end namespace xrtc
