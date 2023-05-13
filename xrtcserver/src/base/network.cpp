#include <ifaddrs.h>

#include <rtc_base/logging.h>

#include "base/network.h"

namespace xrtc {

NetworkManager::~NetworkManager() {
    for (auto network : network_list_) {
        delete network;
    }

    network_list_.clear();
    network_list_.shrink_to_fit();
}

int NetworkManager::create_networks() {
    struct ifaddrs* interface;
    int ret = getifaddrs(&interface);
    if (ret != 0) {
        RTC_LOG(LS_WARNING) << "getifaddrs error: " << strerror(errno) 
            << ", errno: " << errno;
        return -1;
    }

    for (auto cur = interface; cur != nullptr; cur = cur->ifa_next) {
        if (cur->ifa_addr->sa_family != AF_INET) {
            continue;
        }

        struct sockaddr_in* addr = (struct sockaddr_in*)cur->ifa_addr;
        rtc::IPAddress ip_address(addr->sin_addr);

        // 公网环境
        // if (rtc::IPIsPrivateNetwork(ip_address) || rtc::IPIsLoopback(ip_address)) {
        //     continue;
        // }

        // 局域网虚拟机测试
        if (rtc::IPIsLoopback(ip_address)) {
            continue;
        }

        RTC_LOG(LS_INFO) << "gathered network name:" << cur->ifa_name << ", family:" << cur->ifa_addr->sa_family
            << ", ip:" << ip_address.ToString();

        Network* network = new Network(cur->ifa_name, ip_address);

        RTC_LOG(LS_INFO) << "gathered network interface: " << network->to_string();

        network_list_.push_back(network);
    }

    freeifaddrs(interface);

    return 0;
}

}