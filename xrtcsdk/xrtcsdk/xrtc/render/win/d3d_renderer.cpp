//H:\webrtc\webrtc-checkout\src\test\win\d3d_renderer.cc

#include "xrtc/render/win/d3d_renderer.h"

#include <common_video/libyuv/include/webrtc_libyuv.h>
#include <rtc_base/checks.h>
#include <rtc_base/logging.h>
#include <rtc_base/task_utils/to_queued_task.h>
#include <api/video/video_frame.h>

#include "xrtc/base/xrtc_global.h"

namespace xrtc {

std::unique_ptr<VideoRenderer> VideoRenderer::CreatePlatformRenderer(int hwnd,
                                                     size_t width,
                                                     size_t height) 
{
	return D3dRenderer::Create((HWND)hwnd, width, height);
}

D3dRenderer::D3dRenderer(HWND hwnd, size_t width, size_t height)
    : hwnd_(hwnd),
    width_(width),
    height_(height)  
{
  RTC_DCHECK_GT(width, 0);
  RTC_DCHECK_GT(height, 0);

}

D3dRenderer::~D3dRenderer() {
  Destroy();
}

void D3dRenderer::Destroy() {
	RTC_LOG(LS_INFO) << "D3D9RenderSink Destroy";
	if (d3d9_surface_) {
		d3d9_surface_->Release();
		d3d9_surface_ = nullptr;
	}

	if (d3d9_device_) {
		d3d9_device_->Release();
		d3d9_device_ = nullptr;
	}

	if (d3d9_) {
		d3d9_->Release();
		d3d9_ = nullptr;
	}

	if (rgb_buffer_) {
		delete[] rgb_buffer_;
		rgb_buffer_ = nullptr;
		rgb_buffer_size_ = 0;
	}
}

void D3dRenderer::HandleErr(const KRTCError& err) {}

bool D3dRenderer::TryInit(const webrtc::VideoFrame& frame) {
	KRTCError err = KRTCError::kNoErr;

	do {
		if (!d3d9_ || !d3d9_device_ || !d3d9_surface_) {
			break;
		}

		if (width_ != frame.width() ||
			height_ != frame.height())
		{
			break;
		}
		HandleErr(err);
		return true;
	} while (0);

	if (!IsWindow(hwnd_)) {
		if (isInvalidHwnd_) {
			return false;
		}
		isInvalidHwnd_ = true;
		RTC_LOG(LS_WARNING) << "Invalid hwnd";

		HandleErr(KRTCError::kPreviewNoRenderHwndErr);
		return false;
	}

	if (!GetClientRect(hwnd_, &rt_viewport_)) {
		RTC_LOG(LS_WARNING) << "GetClientRect failed, hwnd: " << hwnd_;

		HandleErr(KRTCError::kPreviewNoRenderHwndErr);
		return false;
	}

	// 1. 创建d3d9对象
	if (!d3d9_) {
		d3d9_ = Direct3DCreate9(D3D_SDK_VERSION);
		if (!d3d9_) {
			RTC_LOG(LS_WARNING) << "d3d9 create failed";
			HandleErr(KRTCError::kPreviewCreateRenderDeviceErr);
			return false;
		}
	}

	// 2. 构建d3d9 device
	if (!d3d9_device_) {
		// 创建一个d3d9 device
		D3DPRESENT_PARAMETERS d3dpp;
		ZeroMemory(&d3dpp, sizeof(d3dpp));
		d3dpp.Windowed = true; // 窗口模式
		// 一旦后台缓冲表面的数据被复制到屏幕，那么这个表面的数据就没有用了
		d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
		d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;

		HRESULT res = d3d9_->CreateDevice(
			D3DADAPTER_DEFAULT, // 指定要表示的物理设备，默认主显示器
			D3DDEVTYPE_HAL,     // 支持硬件加速
			hwnd_,              // 渲染窗口句柄                                                    
			D3DCREATE_MIXED_VERTEXPROCESSING | D3DCREATE_MULTITHREADED,// 指定顶点处理方式，混合&多线程
			&d3dpp,
			&d3d9_device_
		);

		if (FAILED(res)) {
			RTC_LOG(LS_WARNING) << "d3d9 create device failed: " << res;
			HandleErr(KRTCError::kPreviewCreateRenderDeviceErr);
			return false;
		}
	}

	// 3. 创建离屏表面
	if (d3d9_surface_) {
		d3d9_surface_->Release();
		d3d9_surface_ = nullptr;
	}

	HRESULT res = res = d3d9_device_->CreateOffscreenPlainSurface(
		frame.width(),						// 离屏表面的宽度
		frame.height(),						// 离屏表面的高度
		D3DFMT_X8R8G8B8,					// 图像像素格式
		D3DPOOL_DEFAULT,					// 资源对应的内存类型
		&d3d9_surface_,						// 返回创建的surface
		NULL
	);

	if (FAILED(res)) {
		RTC_LOG(LS_WARNING) << "d3d9 create offscreen surface failed: " << res;
		HandleErr(KRTCError::kPreviewCreateRenderDeviceErr);
		return false;
	}

	width_ = frame.width();
	height_ = frame.height();

	return true;
}

std::unique_ptr<VideoRenderer> D3dRenderer::Create(
                                 HWND hwnd,
                                 size_t width,
                                 size_t height) 
{
  RTC_LOG(LS_INFO) << "D3dRenderer Create";

  D3dRenderer* d3d_renderer = new D3dRenderer(hwnd, width, height);

  return absl::WrapUnique(d3d_renderer);
}


void D3dRenderer::DoRender(const webrtc::VideoFrame& frame) {
	rtc::scoped_refptr<webrtc::VideoFrameBuffer> vfb = frame.video_frame_buffer();
	if (webrtc::VideoFrameBuffer::Type::kI420 == vfb.get()->type()) {
		// 1. 创建RGB buffer，将YUV格式转换成RGB格式
		int size = frame.width() * frame.height() * 4; // 考虑ARGB，不考虑则是乘以3
		if (rgb_buffer_size_ != size) {		// 避免渲染过程中图像宽高发生变化
			if (rgb_buffer_) {
				delete[] rgb_buffer_;
			}

			rgb_buffer_ = new char[size];
			rgb_buffer_size_ = size;
		}

		ConvertFromI420(frame, webrtc::VideoType::kARGB, 0, (uint8_t*)rgb_buffer_);
	}

	// 2. 将RGB数据拷贝到离屏表面
	// 2.1 锁定区域
	HRESULT res;
	D3DLOCKED_RECT d3d9_rect;
	res = d3d9_surface_->LockRect(&d3d9_rect, // 指向描述锁定区域的矩形结构指针
		NULL, // 需要锁定的区域，如果NULL，表示整个表面
		D3DLOCK_DONOTWAIT // 当显卡锁定区域失败的时候，不阻塞应用程序,返回D3DERR_WASSTILLDRAWING
	);

	if (FAILED(res)) {
		RTC_LOG(LS_WARNING) << "d3d9 surface LockRect failed: " << res;
		return;
	}

	// 2.2 将RGB数据拷贝到离屏表面
	// 锁定区域的地址
	byte* pdest = (byte*)d3d9_rect.pBits;
	// 锁定区域每一行的数据大小
	int stride = d3d9_rect.Pitch;

	if (webrtc::VideoFrameBuffer::Type::kI420 == vfb.get()->type()) {
		int video_width = frame.width();
		int video_height = frame.height();
		int video_stride = video_width * 4;

		if (video_stride == stride) {
			memcpy(pdest, rgb_buffer_, rgb_buffer_size_);
		}
		else if (video_stride < stride) {
			char* src = rgb_buffer_;
			for (int i = 0; i < video_height; ++i) {
				memcpy(pdest, src, video_stride);
				pdest += stride;
				src += video_stride;
			}
		}
	}

	// 2.3 解除锁定
	d3d9_surface_->UnlockRect();

	// 3. 清除后台缓冲
	d3d9_device_->Clear(0,          // 第二个矩形数组参数的大小
		NULL,                       // 需要清除的矩形数组，如果是NULL，表示清除所有
		D3DCLEAR_TARGET,            // 清除渲染目标
		D3DCOLOR_XRGB(30, 30, 30),  // 使用的颜色值
		1.0f, 0);

	// 4. 将离屏表面的数据拷贝到后台缓冲表面
	d3d9_device_->BeginScene();
	// 4.1 获取后台缓冲表面
	IDirect3DSurface9* pback_buffer = nullptr;
	d3d9_device_->GetBackBuffer(0, // 正在使用的交换链的索引
		0,							// 后台缓冲表面的索引
		D3DBACKBUFFER_TYPE_MONO,	// 后台缓冲表面类型
		&pback_buffer);

	// 4.2调整图像和显示区域的宽高比
	// 显示区域的宽高
	float w1 = rt_viewport_.right - rt_viewport_.left;
	float h1 = rt_viewport_.bottom - rt_viewport_.top;
	// 图像的宽高
	float w2 = (float)width_;
	float h2 = (float)height_;
	// 计算目标区域的大小
	int dst_w = 0;
	int dst_h = 0;
	int x, y = 0;

	if (w1 > (w2 * h1) / h2) { // 原始的显示区域，宽度更宽一些
		dst_w = (w2 * h1) / h2;
		dst_h = h1;
		x = (w1 - dst_w) / 2;
		y = 0;
	}
	else {   // 原始的显示区域，高度更高一些
		dst_w = w1;
		dst_h = (h2 * w1) / w2;
		x = 0;
		y = (h1 - dst_h) / 2;
	}

	RECT dest_rect{ x, y, x + dst_w, y + dst_h };

	// 4.3.复制离屏表面的数据到后台缓冲表面
	d3d9_device_->StretchRect(d3d9_surface_, // 源表面
		NULL,           // 源表面的矩形区域，如果是NULL，表示所有区域
		pback_buffer,   // 目标表面
		NULL,//&dest_rect,			// 目标表面的矩形区域，如果NULL，表示所有区域
		D3DTEXF_LINEAR  // 缩放算法，线性插值
	);
	d3d9_device_->EndScene();

	// 5. 显示图像，表面翻转
	d3d9_device_->Present(NULL, // 后台缓冲表面的矩形区域， NULL表示整个区域
		NULL,	// 显示区域， NULL表示整个客户显示区域
		NULL,   // 显示窗口， NULL表示主窗口
		NULL);

	// 6. 释放后台缓冲
	pback_buffer->Release();
}

void D3dRenderer::OnFrame(const webrtc::VideoFrame& frame) {
    // worker_thread执行渲染工作
    XRTCGlobal::Instance()->worker_thread()->PostTask(webrtc::ToQueuedTask([=] {
        if (!TryInit(frame)) {
            return;
        }
        DoRender(frame);
    }));
}

}  // namespace xrtc
