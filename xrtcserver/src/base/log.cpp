#include <iostream>
#include <sys/stat.h>
#include <sys/types.h>

#include "base/log.h"

namespace xrtc {

XRtcLog::XRtcLog(const std::string& log_dir, 
        const std::string& log_name,
        const std::string& log_level) :
    log_dir_(log_dir),
    log_name_(log_name),
    log_level_(log_level),
    log_file_(log_dir + "/" + log_name + ".log"),
    log_wf_file_(log_dir + "/" + log_name + ".log.wf")
{
}

XRtcLog::~XRtcLog() {
    stop();
}

void XRtcLog::OnLogMessage(const std::string& message, rtc::LoggingSeverity severity) {
    if (severity >= rtc::LS_WARNING) {
        std::unique_lock<std::mutex> lock(log_wf_mutex_);
        log_wf_queue_.push(message);
    } else {
        std::unique_lock<std::mutex> lock(log_mutex_);
        log_queue_.push(message);      
    }
}

void XRtcLog::OnLogMessage(const std::string& /*message*/) {
    // 不需要有逻辑
}

static rtc::LoggingSeverity get_log_severity(const std::string& level) {
    if ("verbose" == level) {
        return rtc::LS_VERBOSE;
    } else if ("info" == level) {
        return rtc::LS_INFO;
    } else if ("waring" == level) {
        return rtc::LS_WARNING;
    } else if ("error" == level) {
        return rtc::LS_ERROR;
    } else if ("none" == level) {
        return rtc::LS_NONE;
    } 
    return rtc::LS_NONE;
}

int XRtcLog::init() {
    rtc::LogMessage::ConfigureLogging("thread tstamp"); // 配置现实线程id,相对时间戳
    rtc::LogMessage::SetLogPathPrefix("/src");
    rtc::LogMessage::AddLogToStream(this, get_log_severity(log_level_));

    int ret = mkdir(log_dir_.c_str(), 0755);
    if (ret != 0 && errno != EEXIST) {
        fprintf(stderr, "create log dir[%s] failed \n", log_dir_.c_str());
        return -1;
    }

    out_file_.open(log_file_, std::ios::app);
    if (!out_file_.is_open()) {
        fprintf(stderr, "open log_file_[%s] failed \n", log_file_.c_str());
        return -1;
    }

    out_wf_file_.open(log_wf_file_, std::ios::app);
    if (!out_wf_file_.is_open()) {
        fprintf(stderr, "open log_file_fw_[%s] failed \n", log_wf_file_.c_str());
        return -1;
    }

    return 0;
}

void XRtcLog::set_log_to_stderr(bool on) {
    rtc::LogMessage::SetLogToStderr(on);
}

bool XRtcLog::start() {
    if (running_) {
        fprintf(stderr, "log thread already runing");
        return false;
    }

    running_ = true;

    thread_ = std::make_unique<std::thread>([=] {
        struct stat stat_data;
        std::stringstream ss;

        while(running_) {
            // 检查日记文件是否被删除或者移动
            if (stat(log_file_.c_str(), &stat_data) < 0) {
                out_file_.close();
                out_file_.open(log_file_, std::ios::app);
            }

            if (stat(log_wf_file_.c_str(), &stat_data) < 0) {
                out_wf_file_.close();
                out_wf_file_.open(log_wf_file_, std::ios::app);
            }

            bool write_log = false;
            {
                std::unique_lock<std::mutex> lock(log_mutex_);
                if (!log_queue_.empty()) {
                    write_log = true;
                    while (!log_queue_.empty()) {
                        ss << log_queue_.front();
                        log_queue_.pop();
                    }
                }
            }

            if (write_log) {
                out_file_ << ss.str();
                out_file_.flush();
            }
            ss.str("");

            bool write_wf_log = false;
            {
                std::unique_lock<std::mutex> lock(log_wf_mutex_);
                if (!log_wf_queue_.empty()) {
                    write_wf_log = true;
                    while (!log_wf_queue_.empty()) {
                        ss << log_wf_queue_.front();
                        log_wf_queue_.pop();
                    }
                }
            }

            if (write_wf_log) {
                out_wf_file_ << ss.str();
                out_wf_file_.flush();
            }
            ss.str("");

            std::this_thread::sleep_for(std::chrono::milliseconds(30));
        }
    });

    return true;
}

void XRtcLog::stop() {
    running_ = false;
    if (thread_) {
        if (thread_->joinable()) {
            thread_->join();
        }
        thread_ = nullptr;
    }

    out_file_.close();
    out_wf_file_.close();
}

void XRtcLog::join() {
    if (thread_ && thread_->joinable()) {
        thread_->join();
    }
}

} // namespace xrtc
