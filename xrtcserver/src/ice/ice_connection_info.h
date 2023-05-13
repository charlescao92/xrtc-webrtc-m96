/**
 * @file ice_connection_info.h.h
 * @author charles
 * @brief 
*/

#ifndef  __ICE_CONNECTION_INFO_H_
#define  __ICE_CONNECTION_INFO_H_

namespace xrtc {

enum class IceCandidatePairState {
    WAITING,        // 连通性检查尚未开始
    IN_PROGRESS,    // 检查进行中
    SUCCEEDED,      // 检查成功
    FAILED,         // 检查失败
};

} // end namespace xrtc

#endif  //__ICE_CONNECTION_INFO_H_
