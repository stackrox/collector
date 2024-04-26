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

void Config::Update(const storage::RuntimeFilteringConfiguration& msg) {
  std::unique_lock<std::mutex> lock(mutex_);

  config_message_.emplace(msg);

  condition_.notify_all();
}

bool Config::IsProcessEnabled(uint64_t bitMask) {
  int kProcessFlag = 1;
  return (bitMask & kProcessFlag) != 0;
}

bool Config::IsProcessEnabled(std::string cluster, std::string ns) {
  // Make use of ResourceSelector::IsFeatureEnabledForClusterAndNamespace here.
  // Need to first change the config into something more usable.
  return true;
}

void Config::SetProcessBitMask(bool enabled, std::string container, std::string ns) {
  uint64_t bitMask;
  uint64_t kProcessFlag = 1;

  bitMask = 0;
  bitMask = enabled ? (bitMask | kProcessFlag) : (bitMask & ~kProcessFlag);
  this->containerFeatureBitMask_[container] = &bitMask;
  this->namespaceFeatureBitMask_[container] = &bitMask;
}

bool Config::IsProcessEnabled(std::string cluster, std::string ns, std::string container_id) {
  auto bitMaskPair = containerFeatureBitMask_.find(container_id);
  if (bitMaskPair != containerFeatureBitMask_.end()) {
    uint64_t* bitMask = bitMaskPair->second;
    return IsProcessEnabled(*bitMask);
  } else {
    bitMaskPair = namespaceFeatureBitMask_.find(ns);
    if (bitMaskPair != namespaceFeatureBitMask_.end()) {
      uint64_t* bitMask = bitMaskPair->second;
      containerFeatureBitMask_[container_id] = bitMask;
      return IsProcessEnabled(*bitMask);
    } else {
      bool enabled = IsProcessEnabled(cluster, ns);
      SetProcessBitMask(enabled, cluster, ns);
      return enabled;
    }
  }
}

}  // namespace collector::runtime_control
