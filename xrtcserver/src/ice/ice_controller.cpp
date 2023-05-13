#include <rtc_base/time_utils.h>
#include <rtc_base/logging.h>
#include <absl/algorithm/container.h>

#include "ice/ice_connection.h"
#include "ice/ice_controller.h"

namespace xrtc {

const int k_min_improvement = 10;
const int a_is_better = 1;
const int b_is_better = -1;

void IceController::add_connection(IceConnection* conn) {
    connections_.push_back(conn);
    unpinged_connections_.insert(conn);
}

bool IceController::has_pingable_connection() {
    int64_t now = rtc::TimeMillis();
    for (auto conn : connections_) {
        if (_is_pingable(conn, now)) {
            return true;
        }
    }

    return false;
}

bool IceController::_is_pingable(IceConnection* conn, int64_t now) {
    const Candidate& remote = conn->remote_candidate();
    if (remote.username.empty() || remote.password.empty()) {
        RTC_LOG(LS_WARNING) << "remote ICE ufrag or pwd is empty, cannot ping, "
            << "remote.username:" << remote.username
            << ", remote.password:" << remote.password;
        return false;
    }

    if (_weak()) {
        return true;
    }

    return _is_connection_past_ping_interval(conn, now);
}

PingResult IceController::select_connection_to_ping(int64_t last_ping_sent_ms) {
    bool need_ping_more_at_weak = false;

    for (auto conn : connections_) {
        if (conn->num_pings_sent() < MIN_PINGS_AT_WEAK_PING_INTERVAL) {
            need_ping_more_at_weak = true;
            break;
        }
    }

    int ping_interval = (_weak() || need_ping_more_at_weak) ? WEAK_PING_INTERVAL : STRONG_PING_INTERVAL;

    int64_t now = rtc::TimeMillis();
    const IceConnection* conn = nullptr;
    if (now >= last_ping_sent_ms + ping_interval) {
        conn = _find_next_pingable_connection(now);
    }

    return PingResult(conn, ping_interval);
}

const IceConnection* IceController::_find_next_pingable_connection(int64_t now) {
    if (selected_connection_ && selected_connection_->writable() &&
            _is_connection_past_ping_interval(selected_connection_, now))
    {
        return selected_connection_;
    }

    bool has_pingable = false;
    for (auto conn : unpinged_connections_) {
        if (_is_pingable(conn, now)) {
            has_pingable = true;
            break;
        }
    }
    
    if (!has_pingable) {
        unpinged_connections_.insert(pinged_connections_.begin(), pinged_connections_.end());
        pinged_connections_.clear();
    }

    // 找出一个新可pingable的connection
    IceConnection* find_conn = nullptr;
    for (auto conn : unpinged_connections_) {
        if (!_is_pingable(conn, now)) {
            continue;
        }

        if (_more_pingable(conn, find_conn)) {
            find_conn = conn;
        }
    }

    return find_conn;
}

// 比较conn1和conn2，看conn1是否更适合ping的条件
bool IceController::_more_pingable(IceConnection* conn1, IceConnection* conn2) {
    if (!conn2) {
        return true;
    }

    if (!conn1) {
        return false;
    }
    
    if (conn1->last_ping_sent() < conn2->last_ping_sent()) {
        return true;
    }
    
    if (conn1->last_ping_sent() > conn2->last_ping_sent()) {
        return false;
    }

    return false;
}

// 看时间是否超过了ping的间隔
bool IceController::_is_connection_past_ping_interval(const IceConnection* conn,
        int64_t now)
{
    int interval = _get_connection_ping_interval(conn, now);
    // RTC_LOG(LS_INFO) << "==========conn ping_interval: " << interval 
    //     << ", last_ping_sent: " << conn->last_ping_sent();
    return now >= conn->last_ping_sent() + interval;
}

int IceController::_get_connection_ping_interval(const IceConnection* conn, int64_t now) {
    // 如果 IceConnection 执行 ping 请求的次数 < 3，每隔 48ms 执行一次 ping
    if (conn->num_pings_sent() < MIN_PINGS_AT_WEAK_PING_INTERVAL) {
        return WEAK_PING_INTERVAL;
    }

    // 如果 IceTransportChannel 是 weak 状态，或者该 IceConnection 是不稳定状态，每隔 900ms 执行一次 ping
    if (_weak() || !conn->stable(now)) {
        return STABLING_CONNECTION_PING_INTERVAL;
    }

    // 如果 IceTransportChannel 不是 weak 状态，并且该 IceConnection 是稳定的状态，每隔 2.5s 执行一次 ping
    return STABLE_CONNECTION_PING_INTERVAL;
}

int IceController::_compare_connections(IceConnection* a, IceConnection* b) {
    if (a->writable() && !b->writable()) {
        return a_is_better;
    }
    
    if (!a->writable() && b->writable()) {
        return b_is_better;
    }
    
    if (a->write_state() < b->write_state()) {
        return a_is_better;
    }
    
    if (a->write_state() > b->write_state()) {
        return b_is_better;
    }
    
    if (a->receiving() && !b->receiving()) {
        return a_is_better;
    }
    
    if (!a->receiving() && b->receiving()) {
        return b_is_better;
    }
   
    if (a->priority() > b->priority()) {
        return a_is_better;
    }
    
    if (a->priority() < b->priority()) {
        return b_is_better;
    }

    return 0;
}

bool IceController::ready_to_send(IceConnection* conn) {
    return conn && (conn->writable() || conn->write_state() 
            == IceConnection::STATE_WRITE_UNRELIABLE);
}

IceConnection* IceController::sort_and_switch_connection() {
    absl::c_stable_sort(connections_, [this](IceConnection* conn1, IceConnection* conn2){
        int cmp = _compare_connections(conn1, conn2);
        if (cmp != 0) {
            return cmp > 0;
        }

        return conn1->rtt() < conn2->rtt();
    });

    RTC_LOG(LS_INFO) << "Sort " << connections_.size() << " available connetions";
    for (auto conn : connections_) {
        RTC_LOG(LS_INFO) << conn->to_string();
    }

    IceConnection* top_connection = connections_.empty() ? nullptr : connections_[0];
    if (!ready_to_send(top_connection) || selected_connection_ == top_connection) {
        return nullptr;
    }

    if (!selected_connection_) {
        return top_connection;
    }

    if (top_connection->rtt() <= selected_connection_->rtt() - k_min_improvement) {
        return top_connection;
    }

    return nullptr;
}

void IceController::mark_connection_pinged(IceConnection* conn) {
    if (conn && pinged_connections_.insert(conn).second) {
        unpinged_connections_.erase(conn);
    }
}

void IceController::on_connection_destroyed(IceConnection* conn) {
    pinged_connections_.erase(conn);
    unpinged_connections_.erase(conn);
    
    auto iter = connections_.begin();
    for (; iter != connections_.end(); ++iter) {
        if (*iter == conn) {
            connections_.erase(iter);
            break;
        }
    }
}

} // end namespace xrtc
