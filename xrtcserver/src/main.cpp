#include <memory>
#include <signal.h>

#include "base/log.h"
#include "server/settings.h"
#include "server/signaling_server.h"
#include "server/rtc_server.h"

std::unique_ptr<xrtc::XRtcLog> g_log;
std::unique_ptr<xrtc::SignalingServer> g_signaling_server;
std::unique_ptr<xrtc::RtcServer> g_rtc_server;

int init_log(const xrtc::LogConf& logConf) {
    g_log = std::make_unique<xrtc::XRtcLog>(logConf.log_dir, logConf.log_name, logConf.log_level);

    int ret = g_log->init();
    if (ret != 0) {
        fprintf(stderr, "init log failed \n");
        return -1;
    }

    g_log->set_log_to_stderr(logConf.log_to_stderr);
    g_log->start();

    return 0;
}

int init_signaling_server(const xrtc::SignalingServerOptions& options) {
    g_signaling_server = std::make_unique<xrtc::SignalingServer>();

    int ret = g_signaling_server->init(options);
    if (ret != 0) {
        return -1;
    }

    return 0;
}

int init_rtc_server(const xrtc::RtcServerOptions& options) {
    g_rtc_server = std::make_unique<xrtc::RtcServer>();

    int ret = g_rtc_server->init(options);
    if (ret != 0) {
        return -1;
    }

    return 0;
}

static void process_signal(int sig) {
    RTC_LOG(LS_INFO) << "receive signal:" << sig;
    if (SIGINT == sig || SIGTERM == sig) {
        if (g_signaling_server) {
            g_signaling_server->stop();
        }
        
        if (g_rtc_server) {
            g_rtc_server->stop();
        }
    }
}

int main() {
    if (!xrtc::Singleton<xrtc::Settings>::Instance()->Init("./conf/general.yaml")) {
        fprintf(stderr, "init settings failed \n");
        return -1;
    }

    int ret = init_log(xrtc::Singleton<xrtc::Settings>::Instance()->GetLogConf());
    if (ret != 0) {
        return -1;
    }
    
    ret = init_signaling_server(xrtc::Singleton<xrtc::Settings>::Instance()->GetSignalingServerOptions());
    if (ret != 0) {
        return -1;
    }

    ret = init_rtc_server(xrtc::Singleton<xrtc::Settings>::Instance()->GetRtcServerOptions());
    if (ret != 0) {
        return -1;
    }

    signal(SIGINT, process_signal);
    signal(SIGTERM, process_signal);

    g_signaling_server->start();
    g_rtc_server->start();

    g_signaling_server->join();
    g_rtc_server->join();

    return 0;
}
