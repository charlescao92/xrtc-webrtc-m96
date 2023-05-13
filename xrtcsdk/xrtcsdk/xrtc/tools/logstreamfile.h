#ifndef KRTCSDK_KRTC_TOOLS_LOGSTREAMFILE_H_
#define KRTCSDK_KRTC_TOOLS_LOGSTREAMFILE_H_

#include <string>
#include <memory>

#include <rtc_base/log_sinks.h>

namespace utils {

class LogStreamFile {
 public:
  LogStreamFile(const std::string& logPreffix = "");
  virtual ~LogStreamFile();

  void Init();

 private:
  std::unique_ptr<rtc::FileRotatingLogSink> fileLogSink_ = NULL;
};

} 

#endif // KRTCSDK_KRTC_TOOLS_LOGSTREAMFILE_H_
