#include <runtime-control/Config.h>

#include "Logging.h"

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
  const auto& cluster_scope_config = msg.cluster_scope_config();
  for (const auto& config : cluster_scope_config) {
    auto& default_instance = config.default_instance();
    if (default_instance.feature_case() == storage::CollectorFeature::FeatureCase::kProcesses) {
      auto process_config = reinterpret_cast<const storage::ProcessConfig*>(&default_instance);
      bool process_enabled = process_config->enabled();
      CLOG(INFO) << "process_enabled= " << process_enabled;
    }
  }
  condition_.notify_all();
}

}  // namespace collector::runtime_control
