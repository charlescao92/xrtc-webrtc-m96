#include <sstream>

#include "ice/candidate.h"

namespace xrtc {

/*
priority = (2^24)*(type preference) +
           (2^8)*(local preference) +
           (2^0)*(256 - component ID)
*/
// 0	1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |  NIC Pref     |    Addr Pref  |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// NIC type：3G/wifi
// Addr pref: 定义在RFC3484
// local preference = (NIC Type << 8 | Addr_Pref) + relay preference.
uint32_t Candidate::get_priority(uint32_t type_preference,
        int network_adapter_preference,
        int relay_preference) 
{
    int addr_ref = rtc::IPAddressPrecedence(address.ipaddr());
	int local_pref = ((network_adapter_preference << 8) | addr_ref) + relay_preference;
    return (type_preference << 24) | (local_pref << 8) | (256 - int(component));
}

std::string Candidate::to_string() const {
    std::stringstream ss;
    ss << "Cand[" << foundation << ":" << (int)component << ":" << protocol
        << ":" << priority << ":" << address.ToString() << ":" << type
        << ":" << username << ":" << password << "]";
    return ss.str();
}

}