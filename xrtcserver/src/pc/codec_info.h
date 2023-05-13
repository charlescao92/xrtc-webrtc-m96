
/**
 * @file codec_info.h
 * @author charles
 * @brief 
*/

#ifndef  __CODEC_INFO_H_
#define  __CODEC_INFO_H_

#include <string>
#include <vector>
#include <map>

namespace xrtc {

class AudioCodecInfo;
class VideoCodecInfo;

class FeedbackParam {
public:
    FeedbackParam(const std::string& id, const std::string& param) :
        id_(id), param_(param) {}
    FeedbackParam(const std::string& id) : id_(id), param_("") {}

    std::string id() { return id_; }
    std::string param() { return param_; }

private:
    std::string id_;
    std::string param_;
};

typedef std::map<std::string, std::string> CodecParam;

class CodecInfo {
public:
    virtual AudioCodecInfo* as_audio() { return nullptr; }
    virtual VideoCodecInfo* as_video() { return nullptr; }

public:
    int id;
    std::string name;
    int samplerate;
    std::vector<FeedbackParam> feedback_param;
    CodecParam codec_param;
};

class AudioCodecInfo : public CodecInfo {
public:
    AudioCodecInfo* as_audio() override { return this; }

public:
    int channels;
};

class VideoCodecInfo : public CodecInfo {
public:
    VideoCodecInfo* as_video() override { return this; }
};

} // end namespace xrtc

#endif  //__CODEC_INFO_H_
