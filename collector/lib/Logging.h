#ifndef _LOGGING_H_
#define _LOGGING_H_

#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string.h>

#include "logger.h"

namespace collector {

namespace logging {

enum class LogLevel : uint32_t {
  FATAL = 1,
  CRITICAL = 2,
  ERROR = 3,
  WARNING = 4,
  NOTICE = 5,
  INFO = 6,
  DEBUG = 7,
  TRACE = 8,
};

// Static checks helping to keep the inspector and our logging levels synced.
static_assert(static_cast<int>(LogLevel::TRACE) == sinsp_logger::SEV_TRACE);
static_assert(static_cast<int>(LogLevel::DEBUG) == sinsp_logger::SEV_DEBUG);
static_assert(static_cast<int>(LogLevel::INFO) == sinsp_logger::SEV_INFO);
static_assert(static_cast<int>(LogLevel::NOTICE) == sinsp_logger::SEV_NOTICE);
static_assert(static_cast<int>(LogLevel::WARNING) == sinsp_logger::SEV_WARNING);
static_assert(static_cast<int>(LogLevel::ERROR) == sinsp_logger::SEV_ERROR);
static_assert(static_cast<int>(LogLevel::CRITICAL) == sinsp_logger::SEV_CRITICAL);
static_assert(static_cast<int>(LogLevel::FATAL) == sinsp_logger::SEV_FATAL);
static_assert(static_cast<int>(LogLevel::FATAL) == sinsp_logger::SEV_MIN);
static_assert(static_cast<int>(LogLevel::TRACE) == sinsp_logger::SEV_MAX);

LogLevel GetLogLevel();
void SetLogLevel(LogLevel level);
bool CheckLogLevel(LogLevel level);

const char* GetLogLevelName(LogLevel level);
char GetLogLevelShortName(LogLevel level);
bool ParseLogLevelName(std::string name, LogLevel* level);

void InspectorLogCallback(std::string&& msg, sinsp_logger::severity severity);

const char* GetGlobalLogPrefix();
void SetGlobalLogPrefix(const char* prefix);

void WriteTerminationLog(std::string message);

const size_t LevelPaddingWidth = 7;

class LogMessage {
 public:
  LogMessage(const char* file, int line, bool throttled, LogLevel level)
      : file_(file), line_(line), level_(level), throttled_(throttled) {
    // if in debug mode, output file names associated with log messages
    include_file_ = CheckLogLevel(LogLevel::DEBUG);
  }

  ~LogMessage() {
    auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    auto nowTm = gmtime(&now);

    std::cerr << GetGlobalLogPrefix()
              << "["
              << std::left << std::setw(LevelPaddingWidth) << GetLogLevelName(level_)
              << " " << std::put_time(nowTm, "%Y/%m/%d %H:%M:%S")
              << "] ";

    if (throttled_) {
      std::cerr << "[Throttled] ";
    }

    if (include_file_) {
      const char* basename = strrchr(file_, '/');
      if (!basename) {
        basename = file_;
      } else {
        ++basename;
      }
      std::cerr << "(" << basename << ":" << line_ << ") ";
    }

    std::cerr << buf_.str()
              << std::endl;

    if (level_ == LogLevel::FATAL) {
      WriteTerminationLog(buf_.str());
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
  bool include_file_;
  bool throttled_;
};

}  // namespace logging

}  // namespace collector

#define CLOG_ENABLED(lvl) (collector::logging::CheckLogLevel(collector::logging::LogLevel::lvl))

#define CLOG_IF(cond, lvl)                                                            \
  if (collector::logging::CheckLogLevel(collector::logging::LogLevel::lvl) && (cond)) \
  collector::logging::LogMessage(__FILE__, __LINE__, false, collector::logging::LogLevel::lvl)

#define CLOG(lvl) CLOG_IF(true, lvl)

#define CLOG_THROTTLED_IF(cond, lvl, interval)                                          \
  static std::chrono::steady_clock::time_point _clog_lastlog_##__LINE__;                \
  if (collector::logging::CheckLogLevel(collector::logging::LogLevel::lvl) && (cond) && \
      (std::chrono::steady_clock::now() - _clog_lastlog_##__LINE__ >= interval))        \
  _clog_lastlog_##__LINE__ = std::chrono::steady_clock::now(),                          \
  collector::logging::LogMessage(__FILE__, __LINE__, true, collector::logging::LogLevel::lvl)

#define CLOG_THROTTLED(lvl, interval) CLOG_THROTTLED_IF(true, lvl, interval)

#endif /* _LOG_LEVEL_H_ */
