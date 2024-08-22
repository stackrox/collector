#include <runtime-control/Config.h>

namespace collector::runtime_control {

Config& Config::GetOrCreate() {
  static Config config;

  return config;
}

bool Config::WaitUntilInitialized(unsigned int timeout_ms) {
  std::unique_lock<std::mutex> lock(mutex_);

  return config_message_ || (condition_.wait_for(lock, std::chrono::milliseconds(timeout_ms)) == std::cv_status::no_timeout);
}

void Config::Update(const storage::CollectorConfig& msg) {
  std::unique_lock<std::mutex> lock(mutex_);

  config_message_.emplace(msg);

  condition_.notify_all();
}

}  // namespace collector::runtime_control
