#ifndef KRTCSDK_KRTC_RENDER_WIN_D3D_RENDERER_H_
#define KRTCSDK_KRTC_RENDER_WIN_D3D_RENDERER_H_

#include <Windows.h>
#include <d3d9.h>
#pragma comment(lib, "d3d9.lib")  // located in DirectX SDK

#include <api/media_stream_interface.h>
#include <api/scoped_refptr.h>

#include "xrtc/render/video_renderer.h"
#include "xrtc/xrtc.h"

namespace xrtc {

class D3dRenderer : public VideoRenderer {
public:
    static std::unique_ptr<VideoRenderer> Create(HWND hwnd,
                                            size_t width,
                                            size_t height);
    virtual ~D3dRenderer();

    void OnFrame(const webrtc::VideoFrame& frame) override;

private:
    explicit D3dRenderer(HWND hwnd, size_t width, size_t height);

    bool TryInit(const webrtc::VideoFrame& frame);
    void DoRender(const webrtc::VideoFrame& frame);
    void Destroy();

    void HandleErr(const KRTCError &err);

private:
    HWND hwnd_;
    size_t width_;
    size_t height_;
    IDirect3D9* d3d9_ = nullptr;
    IDirect3DDevice9* d3d9_device_ = nullptr;
    IDirect3DSurface9* d3d9_surface_ = nullptr;

    rtc::scoped_refptr<webrtc::VideoTrackInterface> rendered_track_;
    bool isInvalidHwnd_ = false;
    RECT rt_viewport_ = {0,0,0,0};
    char* rgb_buffer_ = nullptr;
    int rgb_buffer_size_ = 0;
};
}  // namespace xrtc

#endif  // KRTCSDK_KRTC_RENDER_WIN_D3D_RENDERER_H_
