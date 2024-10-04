#ifndef _LOGGING_H_
#define _LOGGING_H_

#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
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

class ILogHeader {
 public:
  ILogHeader(const char* file, int line, LogLevel level)
      : file_(file), line_(line), level_(level) {}
  ILogHeader(const ILogHeader&) = delete;
  ILogHeader(ILogHeader&&) = delete;
  ILogHeader& operator=(const ILogHeader&) = delete;
  ILogHeader& operator=(ILogHeader&&) = delete;
  virtual ~ILogHeader() = default;

  virtual void PrintHeader() = 0;
  inline LogLevel GetLogLevel() const { return level_; }

 protected:
  void PrintPrefix() {
    auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    auto nowTm = gmtime(&now);

    std::cerr << GetGlobalLogPrefix()
              << "["
              << std::left << std::setw(LevelPaddingWidth) << GetLogLevelName(level_)
              << " " << std::put_time(nowTm, "%Y/%m/%d %H:%M:%S")
              << "] ";
  }

  void PrintFile() {
    if (CheckLogLevel(LogLevel::DEBUG)) {
      const char* basename = strrchr(file_, '/');
      if (basename == nullptr) {
        basename = file_;
      } else {
        ++basename;
      }
      std::cerr << "(" << basename << ":" << line_ << ") ";
    }
  }

 private:
  const char* file_;
  int line_;
  LogLevel level_;
};

class LogHeader : public ILogHeader {
 public:
  LogHeader(const char* file, int line, LogLevel level)
      : ILogHeader(file, line, level) {}

  void PrintHeader() override {
    PrintPrefix();
    PrintFile();
  }
};

class ThrottledLogHeader : public ILogHeader {
 public:
  ThrottledLogHeader(const char* file, int line, LogLevel level, std::chrono::duration<unsigned int> interval)
      : ILogHeader(file, line, level), interval_(interval) {}

  void PrintHeader() override {
    PrintPrefix();

    std::cerr << "[Throttled " << count_ << " messages] ";
    count_ = 0;

    PrintFile();
  }

  bool ShouldPrint() {
    std::chrono::duration elapsed = std::chrono::steady_clock::now() - last_log_;
    count_++;
    if (elapsed < interval_) {
      return false;
    }

    last_log_ = std::chrono::steady_clock::now();
    return true;
  }

 private:
  std::chrono::steady_clock::time_point last_log_;
  std::chrono::duration<unsigned int> interval_;
  unsigned long count_{};
};

class LogMessage {
 public:
  LogMessage(ILogHeader& ls) : ls_(ls) {}

  ~LogMessage() {
    ls_.PrintHeader();

    std::cerr << buf_.str()
              << std::endl;

    if (ls_.GetLogLevel() == LogLevel::FATAL) {
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
  ILogHeader& ls_;
  std::stringstream buf_;
};

}  // namespace logging

}  // namespace collector

// Helpers for creating unique variables within a compilation unit.
#define CLOG_CAT_(a, b) a##b
#define CLOG_CAT(a, b) CLOG_CAT_(a, b)
#define CLOG_VAR(a) CLOG_CAT(a, __LINE__)

#define CLOG_ENABLED(lvl) (collector::logging::CheckLogLevel(collector::logging::LogLevel::lvl))

#define CLOG_IF(cond, lvl)                                                            \
  static collector::logging::LogHeader CLOG_VAR(_clog_stmt_)(                         \
      __FILE__, __LINE__, collector::logging::LogLevel::lvl);                         \
  if (collector::logging::CheckLogLevel(collector::logging::LogLevel::lvl) && (cond)) \
  collector::logging::LogMessage(CLOG_VAR(_clog_stmt_))

#define CLOG(lvl) CLOG_IF(true, lvl)

#define CLOG_THROTTLED_IF(cond, lvl, interval)                                          \
  static collector::logging::ThrottledLogHeader CLOG_VAR(_clog_stmt_)(                  \
      __FILE__, __LINE__, collector::logging::LogLevel::lvl, interval);                 \
  if (collector::logging::CheckLogLevel(collector::logging::LogLevel::lvl) && (cond) && \
      CLOG_VAR(_clog_stmt_).ShouldPrint())                                              \
  collector::logging::LogMessage(CLOG_VAR(_clog_stmt_))

#define CLOG_THROTTLED(lvl, interval) CLOG_THROTTLED_IF(true, lvl, interval)

#endif /* _LOG_LEVEL_H_ */
