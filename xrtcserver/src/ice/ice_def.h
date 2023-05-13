/**
 * @file ice_def.h
 * @author charles
 * @brief 
*/

#ifndef  __ICE_DEF_H_
#define  __ICE_DEF_H_

namespace xrtc {

#define LOCAL_PORT_TYPE "host"
#define PRFLX_PORT_TYPE "prflx"

const int ICE_UFRAG_LENGTH = 4;
const int ICE_PWD_LENGTH = 24;

const int STUN_PACKET_SIZE = 60 * 8;
const int WEAK_PING_INTERVAL = 1000 * STUN_PACKET_SIZE / 10000; // 48ms
const int STRONG_PING_INTERVAL = 1000 * STUN_PACKET_SIZE / 1000; // 480ms

const int MIN_PINGS_AT_WEAK_PING_INTERVAL = 3;
const int STABLING_CONNECTION_PING_INTERVAL = 900;
const int STABLE_CONNECTION_PING_INTERVAL = 2500;
const int WEAK_CONNECTION_RECEIVE_TIMEOUT = 2500;
const int CONNECTION_WRITE_CONNECT_FAILS = 5;
const int CONNECTION_WRITE_CONNECT_TIMEOUT = 5000;
const int CONNECTION_WRITE_TIMEOUT = 15000;

enum IceCandidateComponent {
    RTP = 1,
    RTCP = 2
};

// https://datatracker.ietf.org/doc/html/rfc5245#page-9
// 里面推荐的值
enum IcePriorityValue {
    ICE_TYPE_PREFERENCE_RELAY_UDP = 2,
    ICE_TYPE_PREFERENCE_SRFLX = 100,
    ICE_TYPE_PREFERENCE_PRFLX = 110,
    ICE_TYPE_PREFERENCE_HOST = 126,
};

}

#endif // __ICE_DEF_H_
