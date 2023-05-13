/**
 * @file ice_connection.h
 * @author charles
 * @brief 
*/

#ifndef  __ICE_CONNECTION_H_
#define  __ICE_CONNECTION_H_

#include "ice/candidate.h"
#include "ice/stun.h"
#include "ice/ice_credentials.h"
#include "ice/stun_request.h"
#include "ice/ice_connection_info.h"

namespace xrtc {

class EventLoop;
class UDPPort;
class IceConnection;

class ConnectionRequest : public StunRequest {
public:
    ConnectionRequest(IceConnection* conn);
    
protected:
    void prepare(StunMessage* msg) override;
    void on_request_response(StunMessage* msg) override;
    void on_request_error_response(StunMessage* msg) override;

private:
    IceConnection *connection_ = nullptr;
    uint64_t ice_tiebreaker_ = 0;
};

class IceConnection : public sigslot::has_slots<> {
public:
    enum WriteState {
        STATE_WRITABLE = 0,
        STATE_WRITE_UNRELIABLE = 1,
        STATE_WRITE_INIT = 2,
        STATE_WRITE_TIMEOUT = 3
    };

    struct SentPing {
        SentPing(const std::string& id, int64_t timestamp) :
            id(id), sent_time(timestamp) {}

        std::string id;
        int64_t sent_time;
    };

    IceConnection(EventLoop *el, UDPPort *port, const Candidate& remote_candidate);
    ~IceConnection();

public:  
    void on_read_packet(const char* buf, size_t len, int64_t timestamp);
    void handle_stun_binding_request(StunMessage* stun_msg);
    void send_stun_binding_response(StunMessage* stun_msg);
    void maybe_set_remote_ice_params(const IceParameters& ice_params);
    void print_pings_since_last_response(std::string& pings, size_t max);

    void set_write_state(WriteState state);
    WriteState write_state() const { return write_state_; }
    bool writable() const { return write_state_ == STATE_WRITABLE; }
    bool receiving() const { return receiving_; }
    bool weak() { return !(writable() && receiving()); }
    bool active() { return write_state_ != STATE_WRITE_TIMEOUT; }
    bool stable(int64_t now) const;
    void ping(int64_t now);
    void received_ping_response(int rtt);
    void update_receiving(int64_t now);
    int receiving_timeout();
    uint64_t priority();
    int rtt() { return rtt_; }
    void set_selected(bool value) { selected_ = value; }
    bool selected() { return selected_; }

    int64_t last_ping_sent() const { return last_ping_sent_; }
    int64_t last_received();
    int num_pings_sent() const { return num_pings_sent_; }

    void set_state(IceCandidatePairState state);
    IceCandidatePairState state() { return state_; }

    const Candidate& remote_candidate() const { return remote_candidate_; }
    const Candidate& local_candidate() const;
    UDPPort* port() { return port_; }

    void on_connection_request_response(ConnectionRequest* request, StunMessage* msg);
    void on_connection_request_error_response(ConnectionRequest* request, StunMessage* msg);

    std::string to_string();
    void update_state(int64_t now);
    int send_packet(const char* data, size_t len);
    void destroy();

    sigslot::signal1<IceConnection*> signal_state_change;
    sigslot::signal1<IceConnection*> signal_connection_destroy;
    sigslot::signal4<IceConnection*, const char*, size_t, int64_t> signal_read_packet;

private:
    void _send_response_message(const StunMessage& response);
    void _on_stun_send_packet(StunRequest* request, const char* buf, size_t len);
    void _fail_and_destroy();
    bool _miss_response(int64_t now) const;
    bool _too_many_ping_fails(size_t max_pings, int rtt, int64_t now);
    bool _too_long_without_response(int min_time, int64_t now);

private:
    EventLoop *el_ = nullptr;
    UDPPort *port_ = nullptr;
    Candidate remote_candidate_;

    WriteState write_state_ = STATE_WRITE_INIT;
    bool receiving_ = false;
    bool selected_ = false;

    int64_t last_ping_sent_ = 0;
    int64_t last_ping_received_ = 0;
    int64_t last_ping_response_received_ = 0;
    int64_t last_data_received_ = 0;
    int num_pings_sent_ = 0;
    std::vector<SentPing> pings_since_last_response_; //没有收到ping响应的集合
    StunRequestManager requests_;
    int rtt_ = 3000; // 秒    
    int rtt_samples_ = 0; // rtt采样数，实际上是ping response的个数
    IceCandidatePairState state_ = IceCandidatePairState::WAITING;
};

} // end namespace xrtc

#endif  //__ICE_CONNECTION_H_
