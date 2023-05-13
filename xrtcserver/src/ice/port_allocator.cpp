
#include "ice/port_allocator.h"

namespace xrtc {

PortAllocator::PortAllocator() : network_manager_(new NetworkManager()) {
    network_manager_->create_networks();
}

const std::vector<Network*>& PortAllocator::get_networks() {
    return network_manager_->get_networks();
}

void PortAllocator::set_port_range(int min_port, int max_port) {
    if (min_port > 0) {
        min_port_ = min_port;
    }

    if (max_port > 0) {
        max_port_ = max_port;
    }
}

}
