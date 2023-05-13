#include <map>

#include <QVariant>

#include "mainwindow.h"
#include "./ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
   
    initWindow();
}

MainWindow::~MainWindow()
{
	stopPush();
	stopPreview();
	stopDevice();
    delete ui;
}

void MainWindow::initWindow()
{
	setWindowTitle(QStringLiteral("xrtc webrtc新版本推拉流qtdemo"));
    setMinimumSize(1200, 600);
	
	ui->leftWidget->setFixedWidth(250);

	ui->previewOpenGLWidget->setMinimumSize(400,400);
	ui->pullOpenGLWidget->setMinimumSize(400, 400);

    ui->serverLineEdit->setText("https://www.charlescao92.cn:8081");
    ui->uidLineEdit->setText("111");
    ui->pushStreamNamelineEdit->setText("rtc11");
	ui->networkQosLabel->setText("");
	ui->statusLabel->setText("");

	ui->pullStreamNameLineEdit->setText("rtc11");
	ui->pullUidLineEdit->setText("222");
	ui->cameraRadioButton->setChecked(true);

    connect(ui->startDeviceBtn, &QPushButton::clicked,
        this, &MainWindow::onStartDeviceBtnClicked);
    connect(ui->startPreviewBtn, &QPushButton::clicked,
        this, &MainWindow::onStartPreviewBtnClicked);
    connect(ui->startPushBtn, &QPushButton::clicked,
        this, &MainWindow::onStartPushBtnClicked);
    connect(ui->startPullBtn, &QPushButton::clicked,
        this, &MainWindow::onStartPullBtnClicked);
	connect(this, &MainWindow::showToastSignal,
		this, &MainWindow::onShowToast, Qt::QueuedConnection);
	connect(this, &MainWindow::pureVideoFrameSignal,
		this, &MainWindow::OnCapturePureVideoFrameSlots, Qt::QueuedConnection);
	connect(this, &MainWindow::pullVideoFrameSignal,
		this, &MainWindow::OnPullVideoFrameSlots, Qt::QueuedConnection);

	connect(this, &MainWindow::networkInfoSignal,
		[this](const QString& text) {
			ui->networkQosLabel->setText(text);
	});
	connect(this, &MainWindow::startDeviceFailedSignal,
		this, [this] {
			stopDevice();
			ui->startDeviceBtn->setText(QStringLiteral("启动音视频设备"));
		});
	connect(this, &MainWindow::previewFailedSignal,
		[this] {
			stopPreview();
			ui->startPreviewBtn->setText(QStringLiteral("本地预览"));
		});
	connect(this, &MainWindow::pushFailedSignal,
		[this] {
			stopPush();
			ui->startPushBtn->setText(QStringLiteral("开始推流"));
		});
	connect(this, &MainWindow::pullFailedSignal,
		[this] {
			stopPull();
			ui->startPullBtn->setText(QStringLiteral("开始拉流"));
		});

	xrtc::KRTCEngine::Init(this);

	initVideoComboBox();
	initAudioComboBox();

	qRegisterMetaType<MediaFrameSharedPointer>("MediaFrameSharedPointer");

}

void MainWindow::initVideoComboBox()
{
	int total = xrtc::KRTCEngine::GetCameraCount();
	if (total <= 0) {
		return;
	}

	for (int i = 0; i < total; ++i) {
		char device_name[128] = { 0 };
		char device_id[128] = { 0 };
        xrtc::KRTCEngine::GetCameraInfo(i, device_name, sizeof(device_name), device_id, sizeof(device_id));

		QVariant useVar;
		DeviceInfo info = { device_name, device_id };
		useVar.setValue(info);
		ui->videoDeviceCbx->addItem(QString::fromStdString(device_name), useVar);
	}
}

void MainWindow::initAudioComboBox()
{
	int total = xrtc::KRTCEngine::GetMicCount();
	if (total <= 0) {
		return;
	}

	for (int i = 0; i < total; ++i) {
		char device_name[128] = { 0 };
		char device_id[128] = { 0 };
        xrtc::KRTCEngine::GetMicInfo(i, device_name, sizeof(device_name), device_id, sizeof(device_id));

		QVariant useVar;
		DeviceInfo info = { device_name, device_id };
		useVar.setValue(info);
		ui->audioDeviceCbx->addItem(QString::fromStdString(device_name), useVar);
	}
}

void MainWindow::onStartDeviceBtnClicked()
{
	ui->startDeviceBtn->setEnabled(false);
	if (!audio_init_ || !video_init_) {
		if (startDevice()) {
			ui->startDeviceBtn->setText(QStringLiteral("停止音视频设备"));
		}
	}
	else {
		if (stopDevice()) {
			ui->startDeviceBtn->setText(QStringLiteral("启动音视频设备"));
		}
	}
	ui->startDeviceBtn->setEnabled(true);
}

void MainWindow::onStartPreviewBtnClicked()
{
	ui->startPreviewBtn->setEnabled(false);

	if (!xrtc_preview_) {
		if (startPreview()) {
			ui->startPreviewBtn->setText(QStringLiteral("停止预览"));
		}
	}
	else {
		if (stopPreview()) {
			ui->startPreviewBtn->setText(QStringLiteral("本地预览"));
		}
	}

	ui->startPreviewBtn->setEnabled(true);
}

void MainWindow::onStartPushBtnClicked()
{
	ui->startPushBtn->setEnabled(false);
	if (!xrtc_pusher_) {
		if (startPush()) {
			ui->startPushBtn->setText(QStringLiteral("停止推流"));
		}
	}
	else {
		if (stopPush()) {
			ui->startPushBtn->setText(QStringLiteral("开始推流"));
		}
	}

	ui->startPushBtn->setEnabled(true);
}

void MainWindow::onStartPullBtnClicked()
{
	ui->startPullBtn->setEnabled(false);
	if (!xrtc_puller_) {
		if (startPull()) {
			ui->startPullBtn->setText(QStringLiteral("停止拉流"));
		}
	}
	else {
		if (stopPull()) {
			ui->startPullBtn->setText(QStringLiteral("开始拉流"));
		}
	}

	ui->startPullBtn->setEnabled(true);
}

void MainWindow::onShowToast(const QString& toast, bool err)
{
	if (err) {
		ui->statusLabel->setStyleSheet("color:red;");
	}
	else {
		ui->statusLabel->setStyleSheet("color:blue;");
	}
	ui->statusLabel->setText(toast);
}

void MainWindow::OnCapturePureVideoFrameSlots(MediaFrameSharedPointer videoFrame)
{
	ui->previewOpenGLWidget->updateFrame(videoFrame);
}

void MainWindow::OnPullVideoFrameSlots(MediaFrameSharedPointer videoFrame)
{
	ui->pullOpenGLWidget->updateFrame(videoFrame);
}

bool MainWindow::startDevice() {
	if (!video_init_) {
		if (ui->cameraRadioButton->isChecked()) {
			int index = ui->videoDeviceCbx->currentIndex();
			QVariant useVar = ui->videoDeviceCbx->itemData(index);
			std::string device_id = useVar.value<DeviceInfo>().device_id;
			cam_source_ = xrtc::KRTCEngine::CreateCameraSource(device_id.c_str());
			cam_source_->Start();
		}
		else if (ui->desktopRadioButton->isChecked()) {
			desktop_source_ = xrtc::KRTCEngine::CreateScreenSource();
			desktop_source_->Start();
		}
		
	}

	if (!audio_init_) {
		int index = ui->audioDeviceCbx->currentIndex();
		QVariant useVar = ui->audioDeviceCbx->itemData(index);

		std::string device_id = useVar.value<DeviceInfo>().device_id;
		mic_source_ = xrtc::KRTCEngine::CreateMicSource(device_id.c_str());
		mic_source_->Start();
	}

	return true;
}

bool MainWindow::stopDevice() {
	if (video_init_ && cam_source_) {
		cam_source_->Stop();
		cam_source_->Destroy();
		cam_source_ = nullptr;
		video_init_ = false;
	}

	if (video_init_ && desktop_source_) {
		desktop_source_->Stop();
		desktop_source_->Destroy();
		desktop_source_ = nullptr;
		video_init_ = false;
	}

	if (audio_init_ && mic_source_) {
		mic_source_->Stop();
		mic_source_->Destroy();
		mic_source_ = nullptr;
		audio_init_ = false;
	}

	return true;
}

bool MainWindow::startPreview() {
	if (!cam_source_ && !desktop_source_) {
		showToast(QStringLiteral("预览失败：没有视频源"), true);
		return false;
	}

#if 0
	HWND hwnd = (HWND)ui->previewWidget->winId();
	if (!hwnd) {
		showToast(QStringLiteral("预览失败：没有显示窗口"), true);
		return false;
	}
#endif
	
    xrtc_preview_ = xrtc::KRTCEngine::CreatePreview();
	xrtc_preview_->Start();

	return true;
}

bool MainWindow::stopPreview() {
	if (!xrtc_preview_) {
		return false;
	}

	xrtc_preview_->Stop();
	xrtc_preview_ = nullptr;

	showToast(QStringLiteral("停止本地预览成功"), false);

	return true;
}

bool MainWindow::startPush() {
	if (!cam_source_ && !desktop_source_) {
		showToast(QStringLiteral("推流失败：没有视频源"), true);
		return false;
	}

	QString url = ui->serverLineEdit->text();
	if (url.isEmpty()) {
		showToast(QStringLiteral("推流失败：没有服务器地址"), true);
		return false;
	}

	QString strUid = ui->uidLineEdit->text();
	QString streamName = ui->pushStreamNamelineEdit->text();
	xrtc_pusher_ = xrtc::KRTCEngine::CreatePusher(url.toUtf8().constData(), 
		strUid.toUtf8().constData(),
		streamName.toUtf8().constData());
	xrtc_pusher_->Start();

	return true;
}

bool MainWindow::stopPush() {
	if (!xrtc_pusher_) {
		return true;
	}

	xrtc_pusher_->Stop();
	xrtc_pusher_->Destroy();
	xrtc_pusher_ = nullptr;

	return true;
}

bool MainWindow::startPull()
{
	QString url = ui->serverLineEdit->text();
	if (url.isEmpty()) {
		showToast(QStringLiteral("拉流失败：没有服务器地址"), true);
		return false;
	}

	QString strUid = ui->pullUidLineEdit->text();
	QString streamName = ui->pullStreamNameLineEdit->text();
    xrtc_puller_ = xrtc::KRTCEngine::CreatePuller(url.toUtf8().constData(), 
		strUid.toUtf8().constData(),
		streamName.toUtf8().constData());
	xrtc_puller_->Start();

	return true;
}

bool MainWindow::stopPull()
{
	if (!xrtc_puller_) {
		return true;
	}

	xrtc_puller_->Stop();
	xrtc_puller_->Destroy();
	xrtc_puller_ = nullptr;

	return true;
}

void MainWindow::showToast(const QString& toast, bool err)
{
	emit showToastSignal(toast, err);
}

void MainWindow::OnAudioSourceSuccess() {
	audio_init_ = true;
	showToast(QStringLiteral("麦克风启动成功"), false);
}

void MainWindow::OnAudioSourceFailed(xrtc::KRTCError err)
{
	QString err_msg = QStringLiteral("麦克风启动失败: ") + QString::fromStdString(xrtc::KRTCEngine::GetErrString(err));
	showToast(err_msg, true);

	emit startDeviceFailedSignal();
}

void MainWindow::OnVideoSourceSuccess() {
	video_init_ = true;
	showToast(QStringLiteral("摄像头启动成功"), false);
}

void MainWindow::OnVideoSourceFailed(xrtc::KRTCError err)
{
	QString err_msg = QStringLiteral("摄像头启动失败: ") + QString::fromStdString(xrtc::KRTCEngine::GetErrString(err));
	showToast(err_msg, true);
	emit startDeviceFailedSignal();
}

void MainWindow::OnPreviewSuccess() {
	showToast(QStringLiteral("本地预览成功"), false);
}

void MainWindow::OnPreviewFailed(xrtc::KRTCError err) {
	QString err_msg = QStringLiteral("本地预览失败: ") + QString::fromStdString(xrtc::KRTCEngine::GetErrString(err));
	showToast(err_msg, true);

	if (xrtc_preview_) {
		xrtc_preview_->Stop();
		xrtc_preview_ = nullptr;
	}

	emit previewFailedSignal();

}

void MainWindow::OnPushSuccess() {
	showToast(QStringLiteral("推流成功"), false);
}

void MainWindow::OnPushFailed(xrtc::KRTCError err) {
	QString err_msg = QStringLiteral("推流失败: ") + QString::fromStdString(xrtc::KRTCEngine::GetErrString(err));
	showToast(err_msg, true);

	emit pushFailedSignal();
}

void MainWindow::OnPullSuccess() {
	showToast(QStringLiteral("拉流成功"), false);
}

void MainWindow::OnPullFailed(xrtc::KRTCError err) {
	QString err_msg = QStringLiteral("拉流失败: ") + QString::fromStdString(xrtc::KRTCEngine::GetErrString(err));
	showToast(err_msg, true);

	emit pullFailedSignal();
}

void MainWindow::OnCapturePureVideoFrame(std::shared_ptr<xrtc::MediaFrame> frame)
{
	int src_width = frame->fmt.sub_fmt.video_fmt.width;
	int src_height = frame->fmt.sub_fmt.video_fmt.height;
	int stridey = frame->stride[0];
	int strideu = frame->stride[1];
	int stridev = frame->stride[2];
	int size = stridey * src_height + (strideu + stridev) * ((src_height + 1) / 2);

	MediaFrameSharedPointer video_frame(new xrtc::MediaFrame(size));
	video_frame->fmt.sub_fmt.video_fmt.type = xrtc::SubMediaType::kSubTypeI420;
	video_frame->fmt.sub_fmt.video_fmt.width = src_width;
	video_frame->fmt.sub_fmt.video_fmt.height = src_height;
	video_frame->stride[0] = stridey;
	video_frame->stride[1] = strideu;
	video_frame->stride[2] = stridev;
	video_frame->data_len[0] = stridey * src_height;
	video_frame->data_len[1] = strideu * ((src_height + 1) / 2);
	video_frame->data_len[2] = stridev * ((src_height + 1) / 2);
	video_frame->data[1] = video_frame->data[0] + video_frame->data_len[0];
	video_frame->data[2] = video_frame->data[1] + video_frame->data_len[1];
	memcpy(video_frame->data[0], frame->data[0], frame->data_len[0]);
	memcpy(video_frame->data[1], frame->data[1], frame->data_len[1]);
	memcpy(video_frame->data[2], frame->data[2], frame->data_len[2]);

	emit pureVideoFrameSignal(video_frame);
}

void MainWindow::OnPullVideoFrame(std::shared_ptr<xrtc::MediaFrame> frame)
{
	int src_width = frame->fmt.sub_fmt.video_fmt.width;
	int src_height = frame->fmt.sub_fmt.video_fmt.height;
	int stridey = frame->stride[0];
	int strideu = frame->stride[1];
	int stridev = frame->stride[2];
	int size = stridey * src_height + (strideu + stridev) * ((src_height + 1) / 2);

	MediaFrameSharedPointer video_frame(new xrtc::MediaFrame(size));
	video_frame->fmt.sub_fmt.video_fmt.type = xrtc::SubMediaType::kSubTypeI420;
	video_frame->fmt.sub_fmt.video_fmt.width = src_width;
	video_frame->fmt.sub_fmt.video_fmt.height = src_height;
	video_frame->stride[0] = stridey;
	video_frame->stride[1] = strideu;
	video_frame->stride[2] = stridev;
	video_frame->data_len[0] = stridey * src_height;
	video_frame->data_len[1] = strideu * ((src_height + 1) / 2);
	video_frame->data_len[2] = stridev * ((src_height + 1) / 2);
	video_frame->data[1] = video_frame->data[0] + video_frame->data_len[0];
	video_frame->data[2] = video_frame->data[1] + video_frame->data_len[1];
	memcpy(video_frame->data[0], frame->data[0], frame->data_len[0]);
	memcpy(video_frame->data[1], frame->data[1], frame->data_len[1]);
	memcpy(video_frame->data[2], frame->data[2], frame->data_len[2]);

	emit pullVideoFrameSignal(video_frame);
}

void MainWindow::OnNetworkInfo(uint64_t rtt_ms, uint64_t packets_lost, double fraction_lost)
{
	char str[256] = { 0 };
	snprintf(str, sizeof(str), "rtt=%lldms 累计丢包数=%lld 丢包指数=%f",
		rtt_ms, packets_lost, fraction_lost);
	QString networkInfo = QString::fromLocal8Bit(str);
	emit networkInfoSignal(networkInfo);
}

