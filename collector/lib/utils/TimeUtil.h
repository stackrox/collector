#ifndef COLLECTOR_TIME_UTIL_H
#define COLLECTOR_TIME_UTIL_H

#include <chrono>

namespace collector {

// NowMicros returns the current timestamp in microseconds since epoch.
inline int64_t NowMicros() {
  return std::chrono::system_clock::now().time_since_epoch() / std::chrono::microseconds(1);
}

}  // namespace collector

#endif  // COLLECTOR_TIME_UTIL_H
