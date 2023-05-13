#include <rtc_base/logging.h>
#include <absl/algorithm/container.h>
#include <rtc_base/helpers.h>
#include <api/task_queue/default_task_queue_factory.h>

#include "base/event_loop.h"
#include "pc/peer_connection.h"
#include "pc/stream_params.h"
#include "ice/ice_credentials.h"
#include "ice/candidate.h"
#include "modules/rtp_rtcp/rtp_packet.h"

namespace xrtc {

namespace {

struct SsrcInfo {
    uint32_t ssrc_id;
    std::string cname;
    std::string stream_id;
    std::string track_id;
};

static RtpDirection get_direction(bool send, bool recv) {
    if (send && recv) {
        return RtpDirection::k_send_recv;
    } else if (send && !recv) {
        return RtpDirection::k_send_only;
    } else if (!send && recv) {
        return RtpDirection::k_recv_only;
    } else {
        return RtpDirection::k_inactive;
    }
}

}

PeerConnection::PeerConnection(EventLoop* el, PortAllocator *allocator, bool dtls_on) : 
    el_(el),
    transport_controller_(new TransportController(el, allocator, dtls_on)),
    clock_(webrtc::Clock::GetRealTimeClock())
{
    transport_controller_->signal_candidate_allocate_done.connect(this,
        &PeerConnection::_on_candidate_allocate_done);
    transport_controller_->signal_connection_state.connect(this,
        &PeerConnection::_on_connection_state);    
    transport_controller_->signal_rtp_packet_received.connect(this,
            &PeerConnection::_on_rtp_packet_received);
    transport_controller_->signal_rtcp_packet_received.connect(this,
            &PeerConnection::_on_rtcp_packet_received);
}

PeerConnection::~PeerConnection() {
    if (destroy_timer_) {
        el_->delete_timer(destroy_timer_);
        destroy_timer_ = nullptr;
    }

    if (video_recv_stream_) {
        delete video_recv_stream_;
        video_recv_stream_ = nullptr;
    }

    if (audio_recv_stream_) {
        delete audio_recv_stream_;
        audio_recv_stream_ = nullptr;
    }

    RTC_LOG(LS_INFO) << "PeerConnection destroy";   
}

void PeerConnection::_on_rtp_packet_received(TransportController*,
        rtc::CopyOnWriteBuffer* packet, int64_t ts)
{
    signal_rtp_packet_received(this, packet, ts);

    RtpPacket rtp_packet;
    bool success = rtp_packet.Parse((const uint8_t*)packet->data(), packet->size());
    if (!success) return;

    if (remote_audio_ssrc_ == rtp_packet.ssrc() ) {
        if (audio_recv_stream_) {
            audio_recv_stream_->DeliverRtp((const uint8_t*)packet->data(), packet->size());
        }
    } else if(remote_video_ssrc_ == rtp_packet.ssrc() || remote_video_rtx_ssrc_ == rtp_packet.ssrc() ) {
        if (video_recv_stream_) {
            video_recv_stream_->DeliverRtp((const uint8_t*)packet->data(), packet->size());
        }
    }
}

void PeerConnection::_on_rtcp_packet_received(TransportController*,
        rtc::CopyOnWriteBuffer* packet, int64_t ts)
{
    signal_rtcp_packet_received(this, packet, ts);

    if (video_recv_stream_) {
        video_recv_stream_->DeliverRtcp((const uint8_t*)packet->data(), packet->size());
    } else if (audio_recv_stream_) {
        audio_recv_stream_->DeliverRtcp((const uint8_t*)packet->data(), packet->size());
    }
}

void PeerConnection::_on_candidate_allocate_done(TransportController* /*transport_controller*/,
        const std::string& transport_name,
        IceCandidateComponent /*component*/,
        const std::vector<Candidate>& candidates)
{
    for (auto c : candidates) {
        RTC_LOG(LS_INFO) << "candidate gathered, transport_name: " << transport_name
            << ", " << c.to_string();
    }

    if (!local_desc_) {
         return;   
    }

    auto content = local_desc_->get_content(transport_name);
    if (content) {
        content->add_candidates(candidates);
    }
}

void PeerConnection::_on_connection_state(TransportController* /*transport_controller*/, PeerConnectionState state) {
    signal_connection_state(this, state);
    if (state_ != state) {
        state_ = state;
    }
}

int PeerConnection::init(rtc::RTCCertificate* certificate) {
    certificate_ = certificate;
    transport_controller_->set_local_certificate(certificate);
    return 0;
}

std::string PeerConnection::create_answer(const RTCOfferAnswerOptions& options) {
    options_ = options;

    if (options.dtls_on && !certificate_) {
        RTC_LOG(LS_WARNING) << "certificate is null";
        return "";
    }

    local_desc_ = std::make_unique<SessionDescription>(SdpType::k_answer);

    IceParameters ice_param = IceCredentials::create_random_ice_credentials();

    if (exist_push_audio_source_ && (options.recv_audio || options.send_audio)) {
        auto audio = std::make_shared<AudioContentDescription>();
        audio->set_direction(get_direction(options.send_audio, options.recv_audio));
        audio->set_rtcp_mux(options.use_rtcp_mux);
        local_desc_->add_content(audio);
        local_desc_->add_transport_info(audio->mid(), ice_param, certificate_);

        if (options.send_audio) {
            for (auto stream : audio_source_) {
                audio->add_stream(stream);
            }    
        }

    }

    if (exist_push_video_source_ && (options.recv_video || options.send_video)) {
        auto video = std::make_shared<VideoContentDescription>(h264_codec_id_, rtx_codec_id_);
        video->set_direction(get_direction(options.send_video, options.recv_video));
        video->set_rtcp_mux(options.use_rtcp_mux);
        local_desc_->add_content(video);
        local_desc_->add_transport_info(video->mid(), ice_param, certificate_);

        if (options.send_video) {
            for (auto stream : video_source_) {
                video->add_stream(stream);
            }   
        }
    }

    if (options.use_rtp_mux) {
        ContentGroup offer_bundle("BUNDLE");
        for (auto content : local_desc_->contents()) {
            offer_bundle.add_content_name(content->mid());
        }

        if (!offer_bundle.content_names().empty()) {
            local_desc_->add_group(offer_bundle);
        }
    }


    transport_controller_->set_local_description(local_desc_.get());

    // 挪到这里来
    transport_controller_->set_remote_description(remote_desc_.get());
    

    return local_desc_->to_string(options.dtls_on);
}

// 从 a=ice-ufrag:w/W3\r\n 里面取出w/W3
static std::string get_attribute(const std::string& line)
{
    std::vector<std::string> fields;
    size_t size = rtc::tokenize(line, ':', &fields);
    if (size != 2) {
        RTC_LOG(LS_WARNING) << "get attribute error: " << line;
        return "";
    }

    return fields[1];
}

static int parse_transport_info(TransportDescription* td, const std::string& line) 
{
    // a=ice-ufrag:w/W3\r\n
    if (line.find("a=ice-ufrag") != std::string::npos) {
        td->ice_ufrag = get_attribute(line);
        if (td->ice_ufrag.empty()) {
            return -1;
        }

    // a=ice-pwd:l8PkVNNDw9depfNOxAzZHUzT\r\n     
    } else if (line.find("a=ice-pwd") != std::string::npos) {
        td->ice_pwd = get_attribute(line);
        if (td->ice_pwd.empty()) {
            return -1;
        }

    // a=fingerprint:sha-256 A0:C5:56:20:92:76:F7:3E:5D:E7:80:CA:F3:69:51:53:24:3A:CA:16:89:7E:2D:E0:EA:D3:1B:92:7C:D0:EF:B6\r\n
    } else if (line.find("a=fingerprint") != std::string::npos) {
        std::vector<std::string> items;
        rtc::tokenize(line, ' ', &items);
        if (items.size() != 2) {
            RTC_LOG(LS_WARNING) << "parse a=fingerprint error: " << line;
            return -1;
        }

         // 字符串a=fingerprint: 是14字节，还需注意大小写转换
        std::string alg = items[0].substr(14);
        absl::c_transform(alg, alg.begin(), ::tolower);
        std::string content = items[1];

        td->identity_fingerprint = rtc::SSLFingerprint::CreateUniqueFromRfc4572(
                alg, content);
        if (!(td->identity_fingerprint.get())) {
            RTC_LOG(LS_WARNING) << "create fingerprint error: " << line;
            return -1;
        }            

    }
    return 0;
}

static int parse_ssrc_info(std::vector<SsrcInfo>& ssrc_info, const std::string& line) {
    if (line.find("a=ssrc:") == std::string::npos) {
        return 0;
    }

    // rfc5576
    // a=ssrc:<ssrc-id> <attribute>
    // a=ssrc:<ssrc-id> <attribute>:<value>
    std::string field1, field2;
    if (!rtc::tokenize_first(line.substr(2), ' ', &field1, &field2)) {
        RTC_LOG(LS_WARNING) << "parse a=ssrc failed, line: " << line;
        return -1;
    }

    // ssrc:<ssrc-id>
    std::string ssrc_id_s = field1.substr(5);
    uint32_t ssrc_id = 0;
    if (!rtc::FromString(ssrc_id_s, &ssrc_id)) {
        RTC_LOG(LS_WARNING) << "invalid ssrc_id, line: " << line;
        return -1;
    }

    // <attribute>
    std::string attribute;
    std::string value;
    if (!rtc::tokenize_first(field2, ':', &attribute, &value)) {
        RTC_LOG(LS_WARNING) << "get ssrc attribute failed, line: " << line;
        return -1;
    }

    // ssrc_info里面找是否存在ssrc_id的一行
    auto iter = ssrc_info.begin();
    for (; iter != ssrc_info.end(); ++iter) {
        if (iter->ssrc_id == ssrc_id) {
            break;
        }
    }

    // 如果ssrc_info里没有找到ssrc_id的一行，则插入一条ssrc信息
    if (iter == ssrc_info.end()) {
        SsrcInfo info;
        info.ssrc_id = ssrc_id;
        ssrc_info.push_back(info);
        iter = ssrc_info.end() - 1;
    }

    // a=ssrc:3038623782 cname:9UkMttm/AKBk/3gN
    if ("cname" == attribute) {
        iter->cname = value;

    // a=ssrc:3038623782 msid:Z0HUtsuZwWwocPvLkt8PANm3axsdHekKKIXA 19a75650-d2ad-45d8-b7f4-53398701f7de
    } else if ("msid" == attribute) {
        std::vector<std::string> fields;
        rtc::split(value, ' ', &fields);
        if (fields.size() < 1 || fields.size() > 2) {
            RTC_LOG(LS_WARNING) << "msid format error, line: " << line;
            return -1;
        }

        iter->stream_id = fields[0];
        if (fields.size() == 2) {
            iter->track_id = fields[1];
        }
    }

    return 0;
}

// 解析 a=ssrc-group:FID 3038623782 2405544162
static int parse_ssrc_group_info(std::vector<SsrcGroup>& ssrc_groups, const std::string& line) {
    if (line.find("a=ssrc-group:") == std::string::npos) {
        return 0;
    }  

    // rfc5576
    // a=ssrc-group:<semantics> <ssrc-id> ...
    std::vector<std::string> fields;
    rtc::split(line.substr(2), ' ', &fields);
    if (fields.size() < 2) {
        RTC_LOG(LS_WARNING) << "ssrc-group field size < 2, line: " << line;
        return -1;
    }

    std::string semantics = get_attribute(fields[0]);
    if (semantics.empty()) {
        return -1;
    }

    std::vector<uint32_t> ssrcs;
    for (size_t i = 1; i < fields.size(); ++i) {
        uint32_t ssrc_id = 0;
        if (!rtc::FromString(fields[i], &ssrc_id)) {
            return -1;
        }
        ssrcs.push_back(ssrc_id);
    }

    ssrc_groups.push_back(SsrcGroup(semantics, ssrcs));

    return 0;    
}

// 暂时采用比较粗暴的做法
// a=fmtp:100 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f
static int parse_fmtp_info(int &h264_codec_id, const std::string& line) {
    if (h264_codec_id != 0) {
        return 0;
    }

    if (line.find("42e01f") == std::string::npos) {
        return 0;
    }

    if (line.find("level-asymmetry-allowed=1") == std::string::npos) {
        return 0;
    }  

    if (line.find("packetization-mode=1") == std::string::npos) {
        return 0;
    }    

    std::vector<std::string> fields;
    rtc::split(line.substr(2), ' ', &fields);
    if (fields.size() < 2) {
        RTC_LOG(LS_WARNING) << "rtpmap field size < 2, line: " << line;
        return -1;
    }

    std::string code_id = get_attribute(fields[0]);
    if (code_id.empty()) {
        return -1;
    }

    h264_codec_id = atoi(code_id.c_str());
    RTC_LOG(LS_INFO) << "fmtp h264 code id: " << h264_codec_id;

    return 0;
}

// a=rtpmap:101 rtx/90000
static int parse_rtpmap_info(int &rtx_codec_id, const std::string& line) {
    if (rtx_codec_id != 0) {
        return 0;
    }

    if (line.find("rtx/90000") == std::string::npos) {
        return 0;
    }  

    std::vector<std::string> fields;
    rtc::split(line.substr(2), ' ', &fields);
    if (fields.size() < 2) {
        RTC_LOG(LS_WARNING) << "rtpmap field size < 2, line: " << line;
        return -1;
    }

    std::string code_id = get_attribute(fields[0]);
    if (code_id.empty()) {
        return -1;
    }

    rtx_codec_id = atoi(code_id.c_str());
    RTC_LOG(LS_INFO) << "rtpmap rtx code id: " << rtx_codec_id;

    return 0;
}

static void create_track_from_ssrc_info(const std::vector<SsrcInfo>& ssrc_infos,
        std::vector<StreamParams>& tracks) 
{
    for (auto ssrc_info : ssrc_infos) {
        std::string track_id = ssrc_info.track_id;

        auto iter = tracks.begin();
        for (; iter != tracks.end(); ++iter) {
            if (iter->id == track_id) {
                break;
            }
        }

        if (iter == tracks.end()) {
            StreamParams track;
            track.id = track_id;
            tracks.push_back(track);
            iter = tracks.end() - 1;
        }

        iter->cname = ssrc_info.cname;
        iter->stream_id = ssrc_info.stream_id;
        iter->ssrcs.push_back(ssrc_info.ssrc_id);
    }
}

int PeerConnection::set_remote_sdp(const std::string& sdp) {
    std::vector<std::string> fields;
    size_t size = rtc::tokenize(sdp, '\n', &fields);
    if (size <= 0) {
        RTC_LOG(LS_WARNING) << "remote sdp invalid";
        return -1;
    }

    bool is_rn = false;
    if (sdp.find("\r\n") != std::string::npos) {
        is_rn = true;
    }

    remote_desc_ = std::make_unique<SessionDescription>(SdpType::k_offer);

    std::string media_type;
    std::shared_ptr<AudioContentDescription> audio_content;
    std::shared_ptr<VideoContentDescription> video_content;
    auto audio_td = std::make_shared<TransportDescription>();
    auto video_td = std::make_shared<TransportDescription>();

    std::vector<SsrcInfo> audio_ssrc_info;
    std::vector<SsrcInfo> video_ssrc_info;
    std::vector<SsrcGroup> video_ssrc_groups;
    std::vector<StreamParams> audio_tracks;
    std::vector<StreamParams> video_tracks;

    for (auto field : fields) {
        if (is_rn) {
            field = field.substr(0, field.length() - 1);
        }

        if (field.find("m=group:BUNDLE") != std::string::npos) {
            std::vector<std::string> items;
            rtc::split(field, ' ', &items);
            if (items.size() > 1) {
                ContentGroup answer_bundle("BUNDLE");
                for (size_t i = 1; i < items.size(); ++i) {
                    answer_bundle.add_content_name(items[i]);
                }
                remote_desc_->add_group(answer_bundle);
            }
        } else if (field.find("m=") != std::string::npos) {
            std::vector<std::string> items;
            rtc::split(field, ' ', &items);
            if (items.size() <= 2) {
                RTC_LOG(LS_WARNING) << "parse m= error: " << field;
                return -1;
            }

            // m=audio/video
            media_type = items[0].substr(2);
            if ("audio" == media_type) {
                audio_td->mid = "audio";
                exist_push_audio_source_ = true;
            } else if ("video" == media_type){    
                video_td->mid = "video";
                exist_push_video_source_ = true;
            }    
        }

        if ("audio" == media_type) {
            if (parse_transport_info(audio_td.get(), field) != 0) {
                return -1;
            }

            if (parse_ssrc_info(audio_ssrc_info, field) != 0) {
                return -1;
            }

        } else if ("video" == media_type) {
            if (parse_transport_info(video_td.get(), field) != 0) {
                return -1;
            }

            if (parse_ssrc_info(video_ssrc_info, field) != 0) {
                return -1;
            }      

            if (parse_ssrc_group_info(video_ssrc_groups, field) != 0) {
                return -1;
            }

            if (parse_fmtp_info(h264_codec_id_, field) != 0) {
                return -1;
            }
            if (h264_codec_id_ != 0) {
                if (parse_rtpmap_info(rtx_codec_id_, field) != 0) {
                    return -1;
                }      
            }        
        }
    } 

    if (exist_push_audio_source_) {
        audio_content = std::make_shared<AudioContentDescription>();
        remote_desc_->add_content(audio_content);
    }

    if (exist_push_video_source_) {
        video_content = std::make_shared<VideoContentDescription>(h264_codec_id_, rtx_codec_id_);
        remote_desc_->add_content(video_content);
    }

    if (!audio_ssrc_info.empty()) {
        create_track_from_ssrc_info(audio_ssrc_info, audio_tracks);

        for (auto track : audio_tracks) {
            audio_content->add_stream(track);
        }
    }

    if (!video_ssrc_info.empty()) {
        create_track_from_ssrc_info(video_ssrc_info, video_tracks);

        for (auto ssrc_group : video_ssrc_groups) {
            if (ssrc_group.ssrcs.empty()) {
                continue;
            }
            
            uint32_t ssrc = ssrc_group.ssrcs.front();
            for (StreamParams& track : video_tracks) {
                if (track.has_ssrc(ssrc)) {
                    track.ssrc_groups.push_back(ssrc_group);
                }
            }
        }

        for (auto track : video_tracks) {
            video_content->add_stream(track);
        }
    }

    remote_desc_->add_transport_info(audio_td);
    remote_desc_->add_transport_info(video_td);

    if (audio_content) {
        auto audio_codecs = audio_content->get_codecs();
        if (!audio_codecs.empty()) {
            audio_payload_type_ = audio_codecs[0]->id;
        }

        if (options_.recv_audio) {
            _create_audio_receive_stream(audio_content.get());
        }
    }

    if (video_content) {
        auto video_codecs = video_content->get_codecs();
        if (!video_codecs.empty()) {
            video_payload_type_ = video_codecs[0]->id;
        }

        if (video_codecs.size() > 1) {
            video_rtx_payload_type_ = video_codecs[1]->id;
        }

        if (options_.recv_video) {
            _create_video_receive_stream(video_content.get());
        }
    }
  
    //transport_controller_->set_remote_description(remote_desc_.get());

    return 0;
}

void destroy_timer_cb(EventLoop* /*el*/, TimerWatcher* /*w*/, void* data) {
    PeerConnection* pc = (PeerConnection*)data;
    delete pc;
}

void PeerConnection::destroy() {
    if (destroy_timer_) {
        el_->delete_timer(destroy_timer_);
        destroy_timer_ = nullptr;
    }

    destroy_timer_ = el_->create_timer(destroy_timer_cb, this, false);
    el_->start_timer(destroy_timer_, 10000); // 10ms
}

int PeerConnection::send_rtp(const char* data, size_t len) {
    if (transport_controller_) {
        // todo: 需要根据实际情况完善
        // 因为当前是bundle，视频和音频共用一个通道
        return transport_controller_->send_rtp("audio", data, len);
    }

    return -1;
}

int PeerConnection::send_rtcp(const char* data, size_t len) {
    if (transport_controller_) {
        // todo: 需要根据实际情况完善
        // 因为当前是bundle，视频和音频共用一个通道
        return transport_controller_->send_rtcp("audio", data, len);
    }

    return -1;
}

int PeerConnection::send_unencrypted_rtcp(const char* data, size_t len) {
    if (transport_controller_) {
        // 因为当前是bundle，视频和音频共用一个通道
        return transport_controller_->send_unencrypted_rtcp("audio", data, len);
    }

    return -1;
}

void PeerConnection::_create_audio_receive_stream(AudioContentDescription* audio_content) {
    if (!audio_content) {
        return;
    }

    // 暂时只考虑推送一路音频
    for (auto stream : audio_content->streams()) {
        if (!stream.ssrcs.empty()) {
            AudioReceiveStreamConfig config;
            config.rtp.remote_ssrc = stream.ssrcs[0];
            remote_audio_ssrc_ = stream.ssrcs[0];
            config.rtp.local_ssrc = rtc::CreateRandomId(); // 随机32位
            config.rtp.payload_type = audio_payload_type_;
            config.rtp_rtcp_module_observer = this;

            if (!audio_recv_stream_) {
                audio_recv_stream_ = new AudioReceiveStream(el_, clock_, config);
            }
        }
        break;
    }
}

void PeerConnection::_create_video_receive_stream(VideoContentDescription* video_content) {
    if (!video_content) {
        return;
    }

    for (auto stream : video_content->streams()) {
        if (!stream.ssrcs.empty()) {
            VideoReceiveStreamConfig config;
            config.rtp.remote_ssrc = stream.ssrcs[0];
            remote_video_ssrc_ = stream.ssrcs[0];
            config.rtp.local_ssrc = rtc::CreateRandomId(); // 随机32位
            config.rtp.payload_type = video_payload_type_;
            config.rtp_rtcp_module_observer = this;
            if (stream.ssrcs.size() > 1) {
                config.rtp.rtx.ssrc = stream.ssrcs[1];
                remote_video_rtx_ssrc_ = stream.ssrcs[1];
                config.rtp.rtx.payload_type = video_rtx_payload_type_;
            }

            if (!video_recv_stream_) {
                video_recv_stream_ = new VideoReceiveStream(el_, clock_, config);
            }
        }
        break;
    }
}

void PeerConnection::OnLocalRtcpPacket(webrtc::MediaType /*media_type*/, const uint8_t* data, size_t len) {
    if (state_ != PeerConnectionState::k_connected) {
        return;
    }

    send_unencrypted_rtcp((const char*)data, len);
}

void PeerConnection::OnNetworkInfo(int64_t /*rtt_ms*/, int32_t /*packets_lost*/, 
    uint8_t /*fraction_lost*/, uint32_t /*jitter*/) 
{
}

void PeerConnection::OnNackReceived(webrtc::MediaType /*media_type*/, 
const std::vector<uint16_t>& /*nack_list*/) 
{

}
	
void PeerConnection::AddVideoCache(std::shared_ptr<RtpPacketToSend> /*packet*/) {

}

std::shared_ptr<RtpPacketToSend> PeerConnection::FindVideoCache(uint16_t /*seq*/) {
    return nullptr;
}

void PeerConnection::SendPacket(std::unique_ptr<RtpPacketToSend> packet) {
    if (state_ != PeerConnectionState::k_connected) {
        return;
    }

    send_rtcp((const char*)packet->data(), packet->size());
}

} // namespace xrtc
