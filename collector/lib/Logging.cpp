/** collector

A full notice with attributions is provided along with this source code.

This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License version 2 as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program; if not, write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

* In addition, as a special exception, the copyright holders give
* permission to link the code of portions of this program with the
* OpenSSL library under certain conditions as described in each
* individual source file, and distribute linked combinations
* including the two.
* You must obey the GNU General Public License in all respects
* for all of the code used other than OpenSSL.  If you modify
* file(s) with this exception, you may extend this exception to your
* version of the file(s), but you are not obligated to do so.  If you
* do not wish to do so, delete this exception statement from your
* version.
*/

#include "Logging.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <string>
#include <unordered_map>

namespace collector {

namespace logging {

namespace {

std::atomic<uint32_t> g_level(static_cast<uint32_t>(LogLevel::INFO));
std::atomic<const char*> g_log_prefix("");

LogLevel all_levels[] = {
    LogLevel::DEBUG,
    LogLevel::INFO,
    LogLevel::WARNING,
    LogLevel::ERROR,
    LogLevel::FATAL
};

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
  return level >= GetLogLevel();
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

const char* GetGlobalLogPrefix() {
  return g_log_prefix.load(std::memory_order_relaxed);
}

void SetGlobalLogPrefix(const char* prefix) {
  g_log_prefix.store(prefix, std::memory_order_relaxed);
}

}  // namespace logging

}  // namespace collector
