/**
 * @file ice_controller.h
 * @author charles
 * @brief 
*/

#ifndef  __ICE_CONTROLLER_H_
#define  __ICE_CONTROLLER_H_

#include <vector>
#include <set>

namespace xrtc {

class IceTransportChannel;
class IceConnection;

struct PingResult {
    PingResult(const IceConnection* conn, int ping_interval) :
        conn(conn), ping_interval(ping_interval) {}

    const IceConnection* conn = nullptr;
    int ping_interval = 0;
};

class IceController {
public:
    IceController(IceTransportChannel* ice_channel) : ice_channel_(ice_channel) {}
    ~IceController() = default;
  
    void add_connection(IceConnection* conn);
    bool has_pingable_connection();
    PingResult select_connection_to_ping(int64_t last_ping_sent_ms);
    IceConnection* sort_and_switch_connection();
    void set_selected_connection(IceConnection* conn) { selected_connection_ = conn; }
    const std::vector<IceConnection*> connections() { return connections_; }
    void mark_connection_pinged(IceConnection *conn);
    void on_connection_destroyed(IceConnection* conn);
    bool ready_to_send(IceConnection* conn);

private:
    bool _is_pingable(IceConnection* conn, int64_t now);
    bool _weak() {
        return selected_connection_ == nullptr ||  selected_connection_->weak();
    }
    const IceConnection* _find_next_pingable_connection(int64_t last_ping_sent_ms);
    bool _is_connection_past_ping_interval(const IceConnection* conn, int64_t now);
    int _get_connection_ping_interval(const IceConnection* conn, int64_t now);
    bool _more_pingable(IceConnection* conn1, IceConnection* conn2);
    int _compare_connections(IceConnection* a, IceConnection* b);

private:
    IceTransportChannel *ice_channel_;
    IceConnection *selected_connection_ = nullptr;
    std::vector<IceConnection*> connections_;
    std::set<IceConnection*> unpinged_connections_;
    std::set<IceConnection*> pinged_connections_;  
};

} // namespace xrtc

#endif  //__ICE_CONTROLLER_H_


