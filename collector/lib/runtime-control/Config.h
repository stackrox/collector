#ifndef _RUNTIME_CONTROL_CONFIG_H_
#define _RUNTIME_CONTROL_CONFIG_H_

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>

#include <internalapi/sensor/collector_iservice.pb.h>

namespace collector::runtime_control {

class Config {
 public:
  static Config& GetOrCreate();

  // returns true when initialized, false for a timeout.
  bool WaitUntilInitialized(unsigned int timeout_ms);

  void Update(const storage::CollectorConfig& msg);

 private:
  std::mutex mutex_;
  std::condition_variable condition_;
  std::optional<storage::CollectorConfig> config_message_;
};

}  // namespace collector::runtime_control

#endif
