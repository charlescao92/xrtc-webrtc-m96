#ifndef MAINWIDGET_H
#define MAINWIDGET_H

#include <atomic>

#include <QMainWindow>
#include <QMetaType>

#include "xrtc/xrtc.h"
#include "YUVOpenGLWidget.h"

#include "defs.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow,
                   public xrtc::KRTCEngineObserver
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onStartDeviceBtnClicked();
    void onStartPreviewBtnClicked();
    void onStartPushBtnClicked();
    void onStartPullBtnClicked();
    void onShowToast(const QString& toast, bool err);
    void OnCapturePureVideoFrameSlots(MediaFrameSharedPointer videoFrame);
    void OnPullVideoFrameSlots(MediaFrameSharedPointer videoFrame);

signals:
    void showToastSignal(const QString& toast, bool err);
    void networkInfoSignal(const QString& text);
    void startDeviceFailedSignal();
    void previewFailedSignal();
    void pushFailedSignal();
    void pullFailedSignal();
    void pureVideoFrameSignal(MediaFrameSharedPointer videoFrame);
    void pullVideoFrameSignal(MediaFrameSharedPointer videoFrame);

private:
    Ui::MainWindow *ui;
    
    void initWindow();
    void initVideoComboBox();
    void initAudioComboBox();

    bool startDevice();
    bool stopDevice();
    bool startPreview();
    bool stopPreview();
    bool startPush();
    bool stopPush();
    bool startPull();
    bool stopPull();

    void showToast(const QString& toast, bool err);

private:
    // KRTCEngineObserver
    void OnAudioSourceSuccess() override;
    void OnAudioSourceFailed(xrtc::KRTCError err) override;
    void OnVideoSourceSuccess() override;
    void OnVideoSourceFailed(xrtc::KRTCError err) override;
    void OnPreviewSuccess() override;
    void OnPreviewFailed(xrtc::KRTCError err) override;
    void OnNetworkInfo(uint64_t rtt_ms, uint64_t packets_lost, double fraction_lost) override;
    void OnPushSuccess() override;
    void OnPushFailed(xrtc::KRTCError err) override;
    void OnPullSuccess() override;
    void OnPullFailed(xrtc::KRTCError err) override;
    void OnCapturePureVideoFrame(std::shared_ptr<xrtc::MediaFrame> video_frame) override;
    void OnPullVideoFrame(std::shared_ptr<xrtc::MediaFrame> video_frame) override;

private:
    xrtc::IVideoHandler* cam_source_ = nullptr;
    xrtc::IVideoHandler* desktop_source_ = nullptr;
    xrtc::IAudioHandler* mic_source_ = nullptr;
    std::atomic<bool> audio_init_{ false };
    std::atomic<bool> video_init_{ false };
    xrtc::IMediaHandler* xrtc_preview_ = nullptr;
    xrtc::IMediaHandler* xrtc_pusher_ = nullptr;
    xrtc::IMediaHandler* xrtc_puller_ = nullptr;

};

#endif // MAINWINDOW_H
