#ifndef _LOGGING_H_
#define _LOGGING_H_

#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string.h>

namespace collector {

namespace logging {

enum class LogLevel : uint32_t {
  TRACE = 10,
  DEBUG = 20,
  INFO = 30,
  WARNING = 40,
  ERROR = 50,
  FATAL = 60
};

LogLevel GetLogLevel();
void SetLogLevel(LogLevel level);
bool CheckLogLevel(LogLevel level);

const char* GetLogLevelName(LogLevel level);
char GetLogLevelShortName(LogLevel level);
bool ParseLogLevelName(std::string name, LogLevel* level);

const char* GetGlobalLogPrefix();
void SetGlobalLogPrefix(const char* prefix);

class LogMessage {
 public:
  LogMessage(const char* file, int line, LogLevel level)
      : file_(file), line_(line), level_(level) {}

  ~LogMessage() {
    const char* basename = strrchr(file_, '/');
    if (!basename) {
      basename = file_;
    } else {
      ++basename;
    }
    auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    auto nowTm = gmtime(&now);
    std::cerr << GetGlobalLogPrefix()
              << "[" << GetLogLevelShortName(level_)
              << " " << std::put_time(nowTm, "%Y%m%d %H%M%S")
              << " " << basename << ":" << line_ << "] " << buf_.str() << std::endl;
    if (level_ == LogLevel::FATAL) {
      exit(1);
    }
  }

  template <typename T>
  LogMessage& operator<<(const T& arg) {
    buf_ << arg;
    return *this;
  }

 private:
  const char* file_;
  int line_;
  LogLevel level_;
  std::stringstream buf_;
};

}  // namespace logging

}  // namespace collector

#define CLOG_ENABLED(lvl) (collector::logging::CheckLogLevel(collector::logging::LogLevel::lvl))

#define CLOG_IF(cond, lvl)                                                            \
  if (collector::logging::CheckLogLevel(collector::logging::LogLevel::lvl) && (cond)) \
  collector::logging::LogMessage(__FILE__, __LINE__, collector::logging::LogLevel::lvl)

#define CLOG(lvl) CLOG_IF(true, lvl)

#define CLOG_THROTTLED_IF(cond, lvl, interval)                                          \
  static std::chrono::steady_clock::time_point _clog_lastlog_##__LINE__;                \
  if (collector::logging::CheckLogLevel(collector::logging::LogLevel::lvl) && (cond) && \
      (std::chrono::steady_clock::now() - _clog_lastlog_##__LINE__ >= interval))        \
  _clog_lastlog_##__LINE__ = std::chrono::steady_clock::now(),                          \
  collector::logging::LogMessage(__FILE__, __LINE__, collector::logging::LogLevel::lvl)

#define CLOG_THROTTLED(lvl, interval) CLOG_THROTTLED_IF(true, lvl, interval)

#endif /* _LOG_LEVEL_H_ */
