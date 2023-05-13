
#include <sstream>

#include <rtc_base/logging.h>

#include "pc/session_description.h"
#include "ice/ice_def.h"

namespace xrtc {

const char k_media_protocol_dtls_savpf[] = "UDP/TLS/RTP/SAVPF";
const char k_meida_protocol_savpf[] = "RTP/SAVPF";

AudioContentDescription::AudioContentDescription() {
    auto codec = std::make_shared<AudioCodecInfo>();
    codec->id = 111;
    codec->name = "opus";
    codec->samplerate = 48000;
    codec->channels = 2;

    // add feedback param
    codec->feedback_param.push_back(FeedbackParam("transport-cc"));

    // add codec param
    codec->codec_param["minptime"] = "10";
    codec->codec_param["useinbandfec"] = "1";
        
    codecs_.push_back(codec);
}

VideoContentDescription::VideoContentDescription(int h264_codec_id, int rtx_codec_id) {
    auto codec = std::make_shared<VideoCodecInfo>();
    codec->id = h264_codec_id; // 107
    codec->name = "H264";
    codec->samplerate = 90000;
    codecs_.push_back(codec);

    // add feedback param
    codec->feedback_param.push_back(FeedbackParam("goog-remb"));
    codec->feedback_param.push_back(FeedbackParam("transport-cc"));
    codec->feedback_param.push_back(FeedbackParam("ccm", "fir"));
    codec->feedback_param.push_back(FeedbackParam("nack"));
    codec->feedback_param.push_back(FeedbackParam("nack", "pli"));

    // add codec param
    codec->codec_param["level-asymmetry-allowed"] = "1";
    codec->codec_param["packetization-mode"] = "1";
    codec->codec_param["profile-level-id"] = "42e01f";

    auto rtx_codec = std::make_shared<VideoCodecInfo>();
    rtx_codec->id = rtx_codec_id; //99;
    rtx_codec->name = "rtx";
    rtx_codec->samplerate = 90000;

    // add codec param
    rtx_codec->codec_param["apt"] = std::to_string(codec->id);
    codecs_.push_back(rtx_codec);
}

bool ContentGroup::has_content_name(const std::string& content_name) {
    for (auto name : content_names_) {
        if (name == content_name) {
            return true;
        }
    }

    return false;
}

std::shared_ptr<MediaContentDescription> SessionDescription::get_content(
        const std::string& mid)
{
    for (auto content : contents_) {
        if (mid == content->mid()) {
            return content;
        }
    }

    return nullptr;
}

void ContentGroup::add_content_name(const std::string& content_name) {
    if (!has_content_name(content_name)) {
        content_names_.push_back(content_name);
    }
}


SessionDescription::SessionDescription(SdpType type) :
    sdp_type_(type)
{
}

SessionDescription::~SessionDescription() {
}

static void add_fmtp_line(std::shared_ptr<CodecInfo> codec,
        std::stringstream& ss)
{
    if (!codec->codec_param.empty()) {
        ss << "a=fmtp:" << codec->id << " ";
        std::string data;
        for (auto param : codec->codec_param) {
            data += (";" + param.first + "=" + param.second);
        }
        // data = ";key1=value1;key2=value2"
        data = data.substr(1);
        ss << data << "\r\n";
    }
}

static void add_rtcp_fb_line(std::shared_ptr<CodecInfo> codec,
        std::stringstream& ss)
{
    for (auto param : codec->feedback_param) {
        ss << "a=rtcp-fb:" << codec->id << " " << param.id();
        if (!param.param().empty()) {
            ss << " " << param.param();
        }
        ss << "\r\n";
    }
}

static void build_rtp_map(std::shared_ptr<MediaContentDescription> content, 
        std::stringstream& ss) {          
    for (auto codec : content->get_codecs()) {
        ss << "a=rtpmap:" << codec->id << " " << codec->name << "/" << codec->samplerate;
        if (MediaType::MEDIA_TYPE_AUDIO == content->type()) {
            auto audio_codec = codec->as_audio();
            ss << "/" << audio_codec->channels;
        }
        ss << "\r\n";

        add_rtcp_fb_line(codec, ss);
        add_fmtp_line(codec, ss);
    }
}

static void build_rtp_direction(std::shared_ptr<MediaContentDescription> content, std::stringstream& ss) {
    switch (content->direction()) {
        case RtpDirection::k_send_recv:
            ss << "a=sendrecv\r\n";
            break;
        case RtpDirection::k_send_only:
            ss << "a=sendonly\r\n";
            break;
        case RtpDirection::k_recv_only:
            ss << "a=recvonly\r\n";
            break;
        default:
            ss << "a=inactive\r\n";
            break;
    }
}

void SessionDescription::add_content(std::shared_ptr<MediaContentDescription> content) {
    contents_.push_back(content);
}

void SessionDescription::add_group(const ContentGroup& group) {
    content_groups_.push_back(group);
}

std::vector<const ContentGroup*> SessionDescription::get_group_by_name(const std::string& name) const {
    std::vector<const ContentGroup*> content_groups;
    for (const ContentGroup& group : content_groups_) {
        if (group.semantics() == name) {
            content_groups.push_back(&group);
        }
    }

    return content_groups;
}

static std::string connection_role_to_string(ConnectionRole role) {
    switch (role) {
        case ConnectionRole::ACTIVE:
            return "active";
        case ConnectionRole::PASSIVE:
            return "passive";
        case ConnectionRole::ACTPASS:
            return "actpass";
        case ConnectionRole::HOLDCONN:
            return "holdconn";
        default:
            return "none";
    }
}

bool SessionDescription::add_transport_info(const std::string& mid,
        const IceParameters& ice_param,
        rtc::RTCCertificate* certificate) 
{
    auto desc = std::make_shared<TransportDescription>();
    desc->mid = mid;
    desc->ice_ufrag = ice_param.ice_ufrag;
    desc->ice_pwd = ice_param.ice_pwd;

    if (certificate) {
        desc->identity_fingerprint = rtc::SSLFingerprint::CreateFromCertificate(*certificate);
        if (!desc->identity_fingerprint) {
            RTC_LOG(LS_WARNING) << "get fingerprint failed";
            return false;
        }
    }

    if (SdpType::k_offer == sdp_type_) {
        desc->connection_role = ConnectionRole::ACTPASS;
    } else {
        desc->connection_role = ConnectionRole::PASSIVE;
    }

    transport_infos_.emplace_back(std::move(desc));

    return true;
}

bool SessionDescription::add_transport_info(std::shared_ptr<TransportDescription> td) {
    transport_infos_.emplace_back(td);
    return true;
}

std::shared_ptr<TransportDescription> SessionDescription::get_transport_info(const std::string& mid) {
    for (auto info : transport_infos_) {
        if (info->mid == mid) {
            return info;
        }
    }

    return nullptr;
}

bool SessionDescription::is_bundle(const std::string& mid) {
    auto content_group = get_group_by_name("BUNDLE");
    if (content_group.empty()) {
        return false;
    }

    for (auto group : content_group) {
        for (auto name : group->content_names()) {
            if (name == mid) {
                return true;
            }
        }
    }

    return false;
}

std::string SessionDescription::get_first_bundle_mid() {
    auto content_group = get_group_by_name("BUNDLE");
    if (content_group.empty()) {
        return "";
    }
    
    return content_group[0]->content_names()[0];
}

static void build_candidates(std::shared_ptr<MediaContentDescription> content,
        std::stringstream& ss)
{
    for (auto c : content->candidates()) {
        ss << "a=candidate:" << c.foundation
           << " " << c.component
           << " " << c.protocol
           << " " << c.priority
           << " " << c.address.HostAsURIString()
           << " " << c.port
           << " typ " << c.type
           << "\r\n";
    }
}

static void add_ssrc_line(uint32_t ssrc, const std::string& attribute, 
            const std::string& value, std::stringstream& ss) {
    ss << "a=ssrc:" << ssrc << " " << attribute << ":" << value << "\r\n";
}

static void build_ssrc(std::shared_ptr<MediaContentDescription> content, std::stringstream& ss) {
    for (auto track : content->streams()) {
        for (auto ssrc_group : track.ssrc_groups) {
            if (ssrc_group.ssrcs.empty()) {
                continue;
            }

            ss << "a=ssrc-group:" << ssrc_group.semantics;
            for (auto ssrc : ssrc_group.ssrcs) {
                ss << " " << ssrc;
            }
            ss << "\r\n";
        }

        std::string msid = track.stream_id + " " + track.id;
        for (auto ssrc : track.ssrcs) {
            add_ssrc_line(ssrc, "cname", track.cname, ss);
            add_ssrc_line(ssrc, "msid", msid, ss);
            add_ssrc_line(ssrc, "mslabel", track.stream_id, ss);
            add_ssrc_line(ssrc, "lable", track.id, ss);
        }
    }
}

// 当前sdp使用planB方式
std::string SessionDescription::to_string(bool dtls_on) {
    std::stringstream ss;
    // version
    ss << "v=0\r\n";
    // session origin
	// RFC 4566
	// o=<username> <sess-id> <sess-version> <nettype> <addrtype> <unicast-address>
	ss << "o=XRTC/1.0 0 2 IN IP4 127.0.0.1\r\n";
	// session name
	ss << "s=XrtcPublishSession \r\n";
	// time description
	ss << "t=0 0\r\n";

    //改为ice-lite方式，一起是ice-full方式
   // ss << "a=ice-lite\r\n";

    // BUDDLE
    std::vector<const ContentGroup*> content_group = get_group_by_name("BUNDLE");
    if (!content_group.empty()) {
        ss << "a=group:BUNDLE";
        for (auto group : content_group) {
            for (auto content_name : group->content_names()) {
                if (content_name == "audio") {
                    ss << " " << 0;
                } else if (content_name == "video") {
                    ss << " " << 1;
                }    
            }
        }
        ss << "\r\n";
    }

    ss << "a=msid-semantic: WMS  live/xrtc \r\n";

    for (auto content : contents_) {
        // RFC 4566
        // m=<media> <port> <proto> <fmt>
        std::string fmt;
        for (auto codec : content->get_codecs()) {
            fmt.append(" ");
            fmt.append(std::to_string(codec->id));
        }

        std::string meida_protocol;
        if (dtls_on) {
            meida_protocol = k_media_protocol_dtls_savpf;
        } else {
            meida_protocol = k_meida_protocol_savpf;
        }
        ss << "m=" << content->mid() << " 9 " << meida_protocol << fmt << "\r\n";

        ss << "c=IN IP4 0.0.0.0\r\n";
        //ss << "a=rtcp:9 IN IP4 0.0.0.0\r\n";
  
        auto transport_info = get_transport_info(content->mid());
        if (transport_info) {
            ss << "a=ice-ufrag:" << transport_info->ice_ufrag << "\r\n";
            ss << "a=ice-pwd:" << transport_info->ice_pwd << "\r\n";

            auto fp = transport_info->identity_fingerprint.get();
            if (fp) {
                ss << "a=fingerprint:" << fp->algorithm << " " << fp->GetRfc4572Fingerprint()
                    << "\r\n";
                ss << "a=setup:" << connection_role_to_string(
                    transport_info->connection_role) << "\r\n";
            }
        }

        if (content->mid() == "audio") {
            ss << "a=mid:0" << "\r\n";
        } else if (content->mid() == "video") {
            ss << "a=mid:1" << "\r\n";
        }

        ss << "a=extmap:3 http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01" << "\r\n";
        
        build_rtp_direction(content, ss);

        if (content->rtcp_mux()) {
            ss << "a=rtcp-mux\r\n";
        }

        ss << "a=rtcp-rsize\r\n";
        
        build_rtp_map(content, ss);
        build_ssrc(content, ss);

        build_candidates(content, ss);
    }

    return ss.str();
}

} // end namespace xrtc
