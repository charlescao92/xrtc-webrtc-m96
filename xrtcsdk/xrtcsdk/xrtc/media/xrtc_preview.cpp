#include <rtc_base/logging.h>
#include <rtc_base/task_utils/to_queued_task.h>

#include "xrtc/media/default.h"
#include "xrtc/media/xrtc_preview.h"
#include "xrtc/base/xrtc_global.h"
#include "xrtc/base/xrtc_thread.h"
#include "xrtc/render/video_renderer.h"
#include "xrtc/device/vcm_capturer.h"
#include "xrtc/device/desktop_capturer.h"
#include "xrtc/media/media_frame.h"

namespace xrtc {

KRTCPreview::KRTCPreview(const int& hwnd):
    hwnd_(hwnd),
    current_thread_(std::make_unique<KRTCThread>(rtc::Thread::Current()))
{
}

KRTCPreview::~KRTCPreview() = default;


void KRTCPreview::Start() {
    RTC_LOG(LS_INFO) << "KRTCPreview Start call";
    XRTCGlobal::Instance()->worker_thread()->PostTask(webrtc::ToQueuedTask([=]() {
        RTC_LOG(LS_INFO) << "KRTCPreview Start PostTask";

        KRTCError err = KRTCError::kNoErr;

        if (hwnd_ != 0) {
            local_renderer_ = VideoRenderer::Create(CONTROL_TYPE::PUSH, hwnd_, 1, 1);
            if (local_renderer_) {
                webrtc::VideoTrackSource* video_source = XRTCGlobal::Instance()->current_video_source();
                if (video_source) {
                    video_source->AddOrUpdateSink(local_renderer_.get(), rtc::VideoSinkWants());
                }
                else {
                    err = KRTCError::kPreviewNoVideoSourceErr;
                }

            }
            else {
                err = KRTCError::kPreviewCreateRenderDeviceErr;
            }
        }
        else {
            webrtc::VideoTrackSource* video_source = XRTCGlobal::Instance()->current_video_source();
            if (video_source) {
                video_source->AddOrUpdateSink(this, rtc::VideoSinkWants());
            }

            XRTCGlobal::Instance()->SetPreview(true);
        }
      
        if (XRTCGlobal::Instance()->engine_observer()) {
            if (err == KRTCError::kNoErr) {
                XRTCGlobal::Instance()->engine_observer()->OnPreviewSuccess();
            }
            else {
                XRTCGlobal::Instance()->engine_observer()->OnPreviewFailed(err);
            }
        }
    }));
}

void KRTCPreview::Stop() {
    RTC_LOG(LS_INFO) << "KRTCPreview Stop call";
    XRTCGlobal::Instance()->worker_thread()->PostTask(webrtc::ToQueuedTask([=]() {
        RTC_LOG(LS_INFO) << "KRTCPreview Stop PostTask";

        XRTCGlobal::Instance()->SetPreview(false);

        webrtc::VideoTrackSource* video_source = XRTCGlobal::Instance()->current_video_source();
        if (!video_source) {
            return;
        }

        if (local_renderer_) {
            video_source->RemoveSink(local_renderer_.get());
        }
        else {
            video_source->RemoveSink(this);
        }

    }));
}

void KRTCPreview::Destroy() {}

void KRTCPreview::OnFrame(const webrtc::VideoFrame& frame){
    if (XRTCGlobal::Instance()->engine_observer()) {
        rtc::scoped_refptr<webrtc::VideoFrameBuffer> vfb = frame.video_frame_buffer();
        int src_width = frame.width();
        int src_height = frame.height();

        int strideY = vfb->GetI420()->StrideY();
        int strideU = vfb->GetI420()->StrideU();
        int strideV = vfb->GetI420()->StrideV();

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

        // 拿到每个平面数组的指针，然后拷贝数据到平面数组里面
        media_frame->data[0] = new char[media_frame->data_len[0]];
        media_frame->data[1] = new char[media_frame->data_len[1]];
        media_frame->data[2] = new char[media_frame->data_len[2]];
        memcpy(media_frame->data[0], vfb->GetI420()->DataY(), media_frame->data_len[0]);
        memcpy(media_frame->data[1], vfb->GetI420()->DataU(), media_frame->data_len[1]);
        memcpy(media_frame->data[2], vfb->GetI420()->DataV(), media_frame->data_len[2]);

        XRTCGlobal::Instance()->engine_observer()->OnCapturePureVideoFrame(media_frame);
        delete[] media_frame->data[1];
        delete[] media_frame->data[2];
    }

}

} // namespace xrtc