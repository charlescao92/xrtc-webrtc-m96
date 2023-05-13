/**
 * @file network.h
 * @author charles
 * @version 1.0
 * @brief 
*/

#ifndef __BASE_NETWORK_H_
#define __BASE_NETWORK_H_

#include <string>
#include <vector>

#include <rtc_base/ip_address.h>

namespace xrtc {

class Network {
public:
    Network(const std::string& name, const rtc::IPAddress& ip) :
        name_(name), ip_(ip) {}
    ~Network() = default;

public:
    const std::string& name() { 
        return name_;
    }

    const rtc::IPAddress& ip() { 
        return ip_; 
    }

    std::string to_string() {
        return name_ + ":" + ip_.ToString();
    }

private:
    std::string name_;
    rtc::IPAddress ip_;
};

class NetworkManager {
public:
    NetworkManager() = default;
    ~NetworkManager();
   
    const std::vector<Network*>& get_networks() { return network_list_; }
    int create_networks();

private:
    std::vector<Network*> network_list_;
};

} // end namespace xrtc

#endif // __BASE_NETWORK_H_
