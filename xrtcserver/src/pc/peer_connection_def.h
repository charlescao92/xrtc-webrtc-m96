/**
 * @file codec_info.h
 * @author charles
 * @brief 
*/

#ifndef  __PEER_CONNECTION_DEF_H_
#define  __PEER_CONNECTION_DEF_H_

namespace xrtc {

enum class PeerConnectionState {
    k_new = 0,
    k_connecting,
    k_connected,
    k_disconnected,
    k_failed,
    k_closed,
};

} // end namespace xrtc

#endif  // __PEER_CONNECTION_DEF_H_
