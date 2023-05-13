/**
 * @file port_allocator.h
 * @author charles
 * @brief 
*/

#ifndef  __PORT_ALLOCATOR_H_
#define  __PORT_ALLOCATOR_H_

#include <memory>

#include "base/network.h"

namespace xrtc {

class PortAllocator {
public:
    PortAllocator();
    ~PortAllocator() = default;
    
    const std::vector<Network*>& get_networks();

    void set_port_range(int min_port, int max_port);

    int min_port() const { 
        return min_port_; 
    }

    int max_port() const { 
        return max_port_;
    }    

private:
    std::unique_ptr<NetworkManager> network_manager_;
    int min_port_ = 0;
    int max_port_ = 0;
};

} // namespace xrtc

#endif  //__PORT_ALLOCATOR_H_


