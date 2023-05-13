/**
 * @file log.h
 * @author charles
 * @brief 
*/

#ifndef __BASE_LOG_H_
#define __BASE_LOG_H_

#include <fstream>
#include <queue>
#include <mutex>
#include <thread>
#include <memory>

#include <rtc_base/logging.h>

namespace xrtc {

class XRtcLog : public rtc::LogSink {
public:
    XRtcLog(const std::string& log_dir, 
            const std::string& log_name,
            const std::string& log_level);
    ~XRtcLog() override;

public:
    void OnLogMessage(const std::string& message, rtc::LoggingSeverity severity) override;
    void OnLogMessage(const std::string& message) override;

    int init();
    void set_log_to_stderr(bool on);
    bool start();
    void stop();
    void join();

private:
    std::string log_dir_;
    std::string log_name_;
    std::string log_level_;
    std::string log_file_;
    std::string log_wf_file_; //错误日记

    std::ofstream out_file_;
    std::ofstream out_wf_file_;

    std::queue<std::string> log_queue_;
    std::mutex log_mutex_;

    std::queue<std::string> log_wf_queue_;
    std::mutex log_wf_mutex_;

    std::unique_ptr<std::thread> thread_;
    std::atomic<bool> running_{false};
};

} // namespace xrtc

#endif // __BASE_LOG_H_