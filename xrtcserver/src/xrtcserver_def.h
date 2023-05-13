/**
 * @file def.h
 * @author charles
 * @brief 
*/

#ifndef __XRTCSERVER_DEF_H_
#define __XRTCSERVER_DEF_H_

#include <stdint.h>
#include <string>

namespace xrtc {

#define MAX_RES_BUF 4096

#define CMDNO_PUSH     1
#define CMDNO_PULL     2
#define CMDNO_OFFER   3
#define CMDNO_STOPPUSH 4
#define CMDNO_STOPPULL 5

struct RtcMsg {
    int cmdno = -1;
    uint16_t uid = 0;
    std::string stream_name;
    std::string stream_type;
    int audio = 0;
    int video = 0;
    uint32_t log_id = 0;
    void *worker = nullptr;
    void *conn = nullptr;
    int fd = 0;
    std::string sdp;
    int err_no = 0;
    void* certificate = nullptr;
    int dtls_on = 1;
};

} // end namespace xrtc

#endif // __XRTCSERVER_DEF_H_
