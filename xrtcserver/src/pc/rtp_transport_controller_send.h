#ifndef __PC_RTP_TRANSPORT_CONTROLLER_SEND_H_
#define __PC_RTP_TRANSPORT_CONTROLLER_SEND_H_

#include <system_wrappers/include/clock.h>
#include <api/task_queue/task_queue_factory.h>

#include "modules/rtp_rtcp/rtp_packet_to_send.h"
#include "modules/pacing/task_queue_paced_sender.h"

namespace xrtc {

    class RtpTransportControllerSend {
    public:
        RtpTransportControllerSend(webrtc::Clock* clock, 
            PacingController::PacketSender* packet_sender,
            webrtc::TaskQueueFactory* task_queue_factory);
        ~RtpTransportControllerSend();

        void EnqueuePacket(std::unique_ptr<RtpPacketToSend> packet);

    private:
        webrtc::Clock* clock_;
        std::unique_ptr<TaskQueuePacedSender> task_queue_pacer_;
    };

} // end namespace xrtc

#endif // XRTCSDK_XRTC_RTC_PC_RTP_TRANSPORT_CONTROLLER_SEND_H_
