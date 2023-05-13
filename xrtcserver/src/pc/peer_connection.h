/**
 * @file peer_connection.h
 * @author charles
 * @brief 
*/

#ifndef  __PEER_CONNECTION_H_
#define  __PEER_CONNECTION_H_

#include <string>
#include <memory>
#include <vector>

#include <rtc_base/rtc_certificate.h>
#include <rtc_base/third_party/sigslot/sigslot.h>
#include <rtc_base/copy_on_write_buffer.h>

#include "ice/ice_def.h"
#include "pc/transport_controller.h"
#include "pc/stream_params.h"
#include "pc/session_description.h"
#include "pc/rtp_transport_controller_send.h"
#include "audio/audio_receive_stream.h"
#include "video/video_receive_stream.h"
#include "modules/rtp_rtcp/rtp_rtcp_interface.h"

namespace xrtc {

class EventLoop;
class SessionDescription;
class PortAllocator;
class Candidate;

struct RTCOfferAnswerOptions {
    bool send_audio = true;
    bool send_video = true;
    bool recv_audio = true;
    bool recv_video = true;
    bool use_rtp_mux = true;
    bool use_rtcp_mux = true;
    bool dtls_on = true;
};

class PeerConnection : public sigslot::has_slots<>,
                       public RtpRtcpModuleObserver,
                       public PacingController::PacketSender
 {
public:
    PeerConnection(EventLoop* el, PortAllocator *allocator, bool dtls_on);

public: 
    int init(rtc::RTCCertificate* certificate);
    void destroy();
    std::string create_answer(const RTCOfferAnswerOptions& options);
    int set_remote_sdp(const std::string& sdp);

    SessionDescription* remote_desc() { return remote_desc_.get(); }
    SessionDescription* local_desc() { return local_desc_.get(); }

    void add_audio_source(const std::vector<StreamParams>& source) {
        audio_source_ = source;
    }
    
    void add_video_source(const std::vector<StreamParams>& source) {
        video_source_ = source;
    }

    int send_rtp(const char* data, size_t len);
    int send_rtcp(const char* data, size_t len);
    int send_unencrypted_rtcp(const char* data, size_t len);

    sigslot::signal2<PeerConnection*, PeerConnectionState> signal_connection_state;
    sigslot::signal3<PeerConnection*, rtc::CopyOnWriteBuffer*, int64_t> signal_rtp_packet_received;
    sigslot::signal3<PeerConnection*, rtc::CopyOnWriteBuffer*, int64_t> signal_rtcp_packet_received;

private:
    ~PeerConnection();

    void _on_candidate_allocate_done(TransportController* transport_controller,
            const std::string& transport_name,
            IceCandidateComponent component,
            const std::vector<Candidate>& candidate);
    void _on_connection_state(TransportController* transport_controller, PeerConnectionState state);
    void _on_rtp_packet_received(TransportController*, rtc::CopyOnWriteBuffer* packet, int64_t ts);
    void _on_rtcp_packet_received(TransportController*, rtc::CopyOnWriteBuffer* packet, int64_t ts);

    void _create_audio_receive_stream(AudioContentDescription* audio_content);
	void _create_video_receive_stream(VideoContentDescription* video_content);

    friend void destroy_timer_cb(EventLoop* el, TimerWatcher* w, void* data);

    // RtpRtcpModuleObserver
	void OnLocalRtcpPacket(webrtc::MediaType media_type, const uint8_t* data, size_t len) override;
	void OnNetworkInfo(int64_t rtt_ms, int32_t packets_lost, 
                        uint8_t fraction_lost, uint32_t jitter) override;
	void OnNackReceived(webrtc::MediaType media_type, const std::vector<uint16_t>& nack_list) override;
	void AddVideoCache(std::shared_ptr<RtpPacketToSend> packet);
	std::shared_ptr<RtpPacketToSend> FindVideoCache(uint16_t seq);

	// PacingController::PacketSender
	void SendPacket(std::unique_ptr<RtpPacketToSend> packet) override;

private:
    EventLoop *el_= nullptr;
    std::unique_ptr<SessionDescription> local_desc_;
    std::unique_ptr<SessionDescription> remote_desc_;
    rtc::RTCCertificate *certificate_ = nullptr;
    std::unique_ptr<TransportController> transport_controller_;
    TimerWatcher *destroy_timer_ = nullptr;
    std::vector<StreamParams> audio_source_;
    std::vector<StreamParams> video_source_;

    uint32_t remote_audio_ssrc_ = 0;
	uint32_t remote_video_ssrc_ = 0;
	uint32_t remote_video_rtx_ssrc_ = 0;

    uint8_t video_payload_type_ = 0;
	uint8_t video_rtx_payload_type_ = 0;
	uint8_t audio_payload_type_ = 0;

    RTCOfferAnswerOptions options_;

    AudioReceiveStream* audio_recv_stream_ = nullptr;
	VideoReceiveStream* video_recv_stream_ = nullptr;
	webrtc::Clock* clock_;
    PeerConnectionState state_ = PeerConnectionState::k_new;

    bool exist_push_audio_source_ = false;
    bool exist_push_video_source_ = false;
    int h264_codec_id_ = 0;
    int rtx_codec_id_ = 0;
};

} // namespace xrtc

#endif  //__PEER_CONNECTION_H_
