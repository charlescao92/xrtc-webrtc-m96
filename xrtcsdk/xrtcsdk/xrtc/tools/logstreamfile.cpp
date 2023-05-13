#include <stdio.h>
#include <time.h>
#include <iostream>
#include <stdlib.h>

#include "xrtc/tools/logstreamfile.h"

#if defined(_WIN32) || defined(_WIN64)
#include <direct.h>
#include <Windows.h>
#include <ImageHlp.h>
#pragma comment(lib, "imagehlp.lib")
#elif __linux__
#include <unistd.h>
#include <assert.h>
#define MAX_PATH 256
#endif

namespace utils {

LogStreamFile::LogStreamFile(const std::string& logPreffix) {
  std::string binDir;
  char filePathBuffer[MAX_PATH] = {0};
#if WIN32
  GetModuleFileNameA(NULL, filePathBuffer, MAX_PATH);
  (strrchr(filePathBuffer, '\\'))[0] = 0;
  binDir = filePathBuffer;
#else 
  getcwd(filePathBuffer, MAX_PATH);
  binDir = filePathBuffer;
#endif

  std::string logDir = binDir  + "/log";
  RTC_LOG(INFO) << logDir.c_str();

  char ch1[64];
  time_t now = time(NULL);
  struct tm* tm_now;
  tm_now = localtime(&now);
#if defined(_WIN32) || defined(_WIN64)
  sprintf_s(ch1, sizeof(ch1),"%d-%d-%d-%d-%d-%d",
      tm_now->tm_year+1900, 
      tm_now->tm_mon+1, 
      tm_now->tm_mday, 
      tm_now->tm_hour, 
      tm_now->tm_min, 
      tm_now->tm_sec);
#elif __linux__ 
  snprintf(ch1, sizeof(ch1),"%d-%d-%d-%d-%d-%d", 
      tm_now->tm_year+1900, 
      tm_now->tm_mon+1, 
      tm_now->tm_mday, 
      tm_now->tm_hour, 
      tm_now->tm_min, 
      tm_now->tm_sec);
#endif
  std::string suffix = ch1;
  if (!logPreffix.empty()) {
    suffix = logPreffix + "-" + suffix  + ".log";
  } else {
    suffix = suffix + ".log";
  }
  fileLogSink_ = std::make_unique<rtc::FileRotatingLogSink>(
      logDir, suffix, 10 * 1024 * 1024, 10);
  fileLogSink_->Init();

}

LogStreamFile::~LogStreamFile() {
}

void LogStreamFile::Init() {
  rtc::LogMessage::AddLogToStream(fileLogSink_.get(), rtc::INFO);
}

}