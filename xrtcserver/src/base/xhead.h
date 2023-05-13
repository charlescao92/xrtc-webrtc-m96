/**
 * @file conf.h
 * @author charles
 * @version 1.0
 * @brief 
*/

#ifndef __BASE_XHEAD_H_
#define __BASE_XHEAD_H_

#include <stdint.h>

namespace xrtc {

// 协议头大小，固定为36字节
constexpr int XHEAD_SIZE = 36;

// 用来对包进行校验
constexpr uint32_t XHEAD_MAGIC_NUM = 0xfb202212;

struct xhead_t {
    uint16_t id;        // 用来对数据包进行编号
    uint16_t version;   // 协议版本
    uint16_t log_id;    // logid，用来联动上下游，方便定位问题
    char provider[16];  // 用来标记请求来源
    uint32_t magic_num; // 用来对包进行校验
    uint32_t reserved;  // 保留字段，用于扩展
    uint32_t body_len;  // 表示数据体的长度
};

} // end namespace xrtc

#endif // __BASE_XHEAD_H_
