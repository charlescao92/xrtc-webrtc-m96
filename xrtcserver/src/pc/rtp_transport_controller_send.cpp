#include "pc/rtp_transport_controller_send.h"

namespace xrtc {

    RtpTransportControllerSend::RtpTransportControllerSend(webrtc::Clock* clock,
        PacingController::PacketSender* packet_sender,
        webrtc::TaskQueueFactory* task_queue_factory) :
        clock_(clock),
        task_queue_pacer_(std::make_unique<TaskQueuePacedSender>(clock, 
            packet_sender, 
            task_queue_factory,
            webrtc::TimeDelta::Millis(1)))
    {
        task_queue_pacer_->EnsureStarted();
    }

    RtpTransportControllerSend::~RtpTransportControllerSend() {
    }

    void RtpTransportControllerSend::EnqueuePacket(std::unique_ptr<RtpPacketToSend> packet) {
        task_queue_pacer_->EnqueuePacket(std::move(packet));
    }

} // end namespace xrtc
