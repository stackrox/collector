#include "Logging.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <fstream>
#include <string>
#include <unordered_map>

namespace collector {

namespace logging {

namespace {

const char* TerminationLog = "/dev/termination-log";

std::atomic<uint32_t> g_level(static_cast<uint32_t>(LogLevel::INFO));
std::atomic<const char*> g_log_prefix("");

LogLevel all_levels[] = {
    LogLevel::TRACE,
    LogLevel::DEBUG,
    LogLevel::INFO,
    LogLevel::WARNING,
    LogLevel::ERROR,
    LogLevel::FATAL};

std::unordered_map<std::string, LogLevel>& GetNameToLevelMap() {
  static std::unordered_map<std::string, LogLevel>* name_to_level_map = []() {
    std::unordered_map<std::string, LogLevel>* map = new std::unordered_map<std::string, LogLevel>;
    for (LogLevel level : all_levels) {
      map->emplace(GetLogLevelName(level), level);
    }
    return map;
  }();
  return *name_to_level_map;
}

}  // namespace

LogLevel GetLogLevel() {
  return static_cast<LogLevel>(g_level.load(std::memory_order_relaxed));
}

void SetLogLevel(LogLevel level) {
  g_level.store(static_cast<int32_t>(level), std::memory_order_relaxed);
}

bool CheckLogLevel(LogLevel level) {
  return level <= GetLogLevel();
}

const char* GetLogLevelName(LogLevel level) {
  switch (level) {
    case LogLevel::TRACE:
      return "TRACE";
    case LogLevel::DEBUG:
      return "DEBUG";
    case LogLevel::INFO:
      return "INFO";
    case LogLevel::WARNING:
      return "WARNING";
    case LogLevel::ERROR:
      return "ERROR";
    case LogLevel::FATAL:
      return "FATAL";
    default:
      return "UNKNOWN";
  };
}

char GetLogLevelShortName(LogLevel level) {
  switch (level) {
    case LogLevel::TRACE:
      return 'T';
    case LogLevel::DEBUG:
      return 'D';
    case LogLevel::INFO:
      return 'I';
    case LogLevel::WARNING:
      return 'W';
    case LogLevel::ERROR:
      return 'E';
    case LogLevel::FATAL:
      return 'F';
    default:
      return 'U';
  };
}

bool ParseLogLevelName(std::string name, LogLevel* level) {
  const auto& name_to_level_map = GetNameToLevelMap();
  std::transform(name.begin(), name.end(), name.begin(), [](char c) { return std::toupper(c); });
  auto it = name_to_level_map.find(name);
  if (it == name_to_level_map.end()) {
    return false;
  }
  *level = it->second;
  return true;
}

void InspectorLogCallback(std::string&& msg, sinsp_logger::severity severity) {
  auto collector_severity = (LogLevel)severity;
  collector::logging::LogMessage(__FILE__, __LINE__, false, collector_severity) << msg;
}

const char* GetGlobalLogPrefix() {
  return g_log_prefix.load(std::memory_order_relaxed);
}

void SetGlobalLogPrefix(const char* prefix) {
  g_log_prefix.store(prefix, std::memory_order_relaxed);
}

void WriteTerminationLog(std::string message) {
  std::ofstream tlog(TerminationLog);

  // If the termination log does not exist it is likely that
  // we're not running in k8s so otherwise do nothing. This
  // diagnostic measure does not work in openshift.
  if (tlog.is_open()) {
    tlog.write(message.c_str(), message.size());
  }
}

}  // namespace logging

}  // namespace collector
