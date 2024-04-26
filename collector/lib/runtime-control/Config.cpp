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

void Config::Update(const storage::RuntimeFilteringConfiguration& msg) {
  std::unique_lock<std::mutex> lock(mutex_);

  config_message_.emplace(msg);

  condition_.notify_all();
}

bool Config::IsProcessEnabled(uint64_t bitMask) {
  CLOG(INFO) << "bitMask= " << bitMask;
  int kProcessFlag = 1;
  return (bitMask & kProcessFlag) != 0;
}

void Config::ConfigMessageToConfig(const storage::RuntimeFilteringConfiguration& msg) {
  for (storage::RuntimeFilter runtimeConfig : msg.runtime_filters()) {
    if (runtimeConfig.default_status().compare("on") == 0) {
      default_status_map_[runtimeConfig.feature()] = true;
    } else {
      default_status_map_[runtimeConfig.feature()] = false;
    }

    for (auto runtimeConfigRule : runtimeConfig.rules()) {
      config_by_feature_[runtimeConfig.feature()].push_back(runtimeConfigRule);
    }
  }

  for (storage::ResourceCollection rc : msg.resource_collections()) {
    rcMap_[rc.id()] = rc;
  }
}

bool Config::IsProcessEnabled(std::string cluster, std::string ns) {
  auto filteringRulesPair = config_by_feature_.find(storage::RuntimeFilter_RuntimeFilterFeatures_PROCESSES);
  if (filteringRulesPair != config_by_feature_.end()) {
    bool defaultStatus = default_status_map_[storage::RuntimeFilter_RuntimeFilterFeatures_PROCESSES];
    auto filteringRules = filteringRulesPair->second;
    return ResourceSelector::IsFeatureEnabledForClusterAndNamespace(filteringRules, rcMap_, defaultStatus, cluster, ns);
  } else {
    return false;
  }
}

void Config::SetProcessBitMask(bool enabled, std::string container_id, std::string ns) {
  uint64_t bitMask;
  uint64_t kProcessFlag = 1;

  bitMask = 0;
  bitMask = enabled ? (bitMask | kProcessFlag) : (bitMask & ~kProcessFlag);
  CLOG(INFO) << "bitMask= " << bitMask;
  namespaceFeatureBitMask_[ns] = bitMask;
  containerFeatureBitMask_[container_id] = &namespaceFeatureBitMask_[ns];
  CLOG(INFO) << "namespaceFeatureBitMask_[ns]= " << namespaceFeatureBitMask_[ns];
  CLOG(INFO) << "*containerFeatureBitMask_[container_id]= " << *containerFeatureBitMask_[container_id];
}

bool Config::IsProcessEnabled(std::string cluster, std::string ns, std::string container_id) {
  CLOG(INFO) << "In IsProcessEnabled";
  auto bitMaskPair = containerFeatureBitMask_.find(container_id);
  if (bitMaskPair != containerFeatureBitMask_.end()) {
    CLOG(INFO) << "Found in container map";
    uint64_t* bitMask = bitMaskPair->second;
    return IsProcessEnabled(*bitMask);
  } else {
    auto bitMaskPairNs = namespaceFeatureBitMask_.find(ns);
    if (bitMaskPairNs != namespaceFeatureBitMask_.end()) {
      CLOG(INFO) << "Found in namespace map";
      uint64_t bitMask = bitMaskPairNs->second;
      containerFeatureBitMask_[container_id] = &namespaceFeatureBitMask_[ns];
      return IsProcessEnabled(bitMask);
    } else {
      bool enabled = IsProcessEnabled(cluster, ns);
      CLOG(INFO) << "enabled= " << enabled;
      SetProcessBitMask(enabled, container_id, ns);
      return enabled;
    }
  }
}

}  // namespace collector::runtime_control
