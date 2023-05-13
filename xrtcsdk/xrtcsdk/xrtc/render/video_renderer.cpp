// H:\webrtc\webrtc-checkout\src\test\video_renderer.cc

#include "xrtc/render/video_renderer.h"
#include "xrtc/media/media_frame.h"
#include "xrtc/base/xrtc_global.h"

namespace xrtc {

class NullRenderer : public VideoRenderer {
public:
    NullRenderer(CONTROL_TYPE type):
        type_(type){}

private:
    void OnFrame(const webrtc::VideoFrame& video_frame) override {
        if (type_ == CONTROL_TYPE::PUSH) {
            return;
        }

        if (XRTCGlobal::Instance()->engine_observer()) {
            rtc::scoped_refptr<webrtc::VideoFrameBuffer> vfb = video_frame.video_frame_buffer();
            int src_width = video_frame.width();
            int src_height = video_frame.height();

            int strideY = vfb.get()->GetI420()->StrideY();
            int strideU = vfb.get()->GetI420()->StrideU();
            int strideV = vfb.get()->GetI420()->StrideV();

            int size = strideY * src_height + (strideU + strideV) * ((src_height + 1) / 2);
            std::shared_ptr<MediaFrame> media_frame = std::make_shared<MediaFrame>(size);
            media_frame->fmt.media_type = MainMediaType::kMainTypeVideo;
            media_frame->fmt.sub_fmt.video_fmt.type = SubMediaType::kSubTypeI420;
            media_frame->fmt.sub_fmt.video_fmt.width = src_width;
            media_frame->fmt.sub_fmt.video_fmt.height = src_height;
            media_frame->stride[0] = strideY;
            media_frame->stride[1] = strideU;
            media_frame->stride[2] = strideV;
            media_frame->data_len[0] = strideY * src_height;
            media_frame->data_len[1] = strideU * ((src_height + 1) / 2);
            media_frame->data_len[2] = strideV * ((src_height + 1) / 2);

            media_frame->data[1] = media_frame->data[0] + media_frame->data_len[0];
            media_frame->data[2] = media_frame->data[1] + media_frame->data_len[1];
            memcpy(media_frame->data[0], video_frame.video_frame_buffer()->GetI420()->DataY(), media_frame->data_len[0]);
            memcpy(media_frame->data[1], video_frame.video_frame_buffer()->GetI420()->DataU(), media_frame->data_len[1]);
            memcpy(media_frame->data[2], video_frame.video_frame_buffer()->GetI420()->DataV(), media_frame->data_len[2]);

            XRTCGlobal::Instance()->engine_observer()->OnPullVideoFrame(media_frame);
        }
    }

private:
    CONTROL_TYPE type_;
};

std::unique_ptr<VideoRenderer> VideoRenderer::Create(CONTROL_TYPE type, int hwnd, size_t width, size_t height)
{
#if defined(_WIN32) || defined(_WIN64)
    if (0 != hwnd) {
        std::unique_ptr<VideoRenderer> renderer = CreatePlatformRenderer(hwnd, width, height);
        if (renderer != nullptr)
            return std::move(renderer);
    }
#endif

    return std::make_unique<NullRenderer>(type);
}

}  // namespace xrtc
