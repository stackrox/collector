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
  ConfigMessageToConfig(msg);
  std::string cluster = "cluster-1";  // Cluster is always the same, so should probably not use it anywhere. Using it but hardcoding it for now.
  SetBitMasksForNamespaces(cluster);

  condition_.notify_all();
}

bool Config::IsFeatureEnabled(uint64_t bitMask, storage::RuntimeFilterFeatures feature) {
  CLOG(INFO) << "bitMask= " << bitMask;
  return (bitMask & kFeatureFlags_[feature]) != 0;
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

bool Config::IsFeatureEnabled(std::string cluster, std::string ns, storage::RuntimeFilterFeatures feature) {
  auto filteringRulesPair = config_by_feature_.find(feature);
  if (filteringRulesPair != config_by_feature_.end()) {
    bool defaultStatus = default_status_map_[feature];
    auto filteringRules = filteringRulesPair->second;
    return ResourceSelector::IsFeatureEnabledForClusterAndNamespace(filteringRules, rcMap_, defaultStatus, cluster, ns);
  } else {
    return false;
  }
}

uint64_t Config::SetFeatureBitMask(uint64_t bitMask, bool enabled, storage::RuntimeFilterFeatures feature) {
  bitMask = enabled ? (bitMask | kFeatureFlags_[feature]) : (bitMask & ~kFeatureFlags_[feature]);
  CLOG(INFO) << "bitMask= " << bitMask;
  return bitMask;
}

void Config::SetBitMask(std::string cluster, std::string ns) {
  uint64_t bitMask = 0;
  for (int i = 0; i < storage::RuntimeFilterFeatures_ARRAYSIZE; i++) {
    storage::RuntimeFilterFeatures feature = static_cast<storage::RuntimeFilterFeatures>(i);
    bool enabled = IsFeatureEnabled(cluster, ns, feature);
    bitMask = SetFeatureBitMask(bitMask, enabled, feature);
  }
  namespaceFeatureBitMask_[ns] = bitMask;
}

void Config::SetBitMasksForNamespaces(std::string cluster) {
  for (auto it = namespaceFeatureBitMask_.begin(); it != namespaceFeatureBitMask_.end();) {
    auto& pair = *it;
    std::string ns = pair.first;
    SetBitMask(cluster, ns);
  }
}

void Config::SetBitMask(std::string cluster, std::string container_id, std::string ns) {
  SetBitMask(cluster, ns);
  containerFeatureBitMask_[container_id] = &namespaceFeatureBitMask_[ns];
  CLOG(INFO) << "namespaceFeatureBitMask_[ns]= " << namespaceFeatureBitMask_[ns];
  CLOG(INFO) << "*containerFeatureBitMask_[container_id]= " << *containerFeatureBitMask_[container_id];
}

// Should probably just take in the container_id, check the containerFeatureBitMask_ map, and only figure out the namespace if the container is not in the map.
// For now this is easier for testing purposes.
bool Config::IsFeatureEnabled(std::string cluster, std::string ns, std::string container_id, storage::RuntimeFilterFeatures feature) {
  auto bitMaskPair = containerFeatureBitMask_.find(container_id);
  if (bitMaskPair != containerFeatureBitMask_.end()) {
    CLOG(INFO) << "Found in container map";
    uint64_t* bitMask = bitMaskPair->second;
    return IsFeatureEnabled(*bitMask, feature);
  } else {
    auto bitMaskPairNs = namespaceFeatureBitMask_.find(ns);
    if (bitMaskPairNs != namespaceFeatureBitMask_.end()) {
      CLOG(INFO) << "Found in namespace map";
      uint64_t bitMask = bitMaskPairNs->second;
      containerFeatureBitMask_[container_id] = &namespaceFeatureBitMask_[ns];
      return IsFeatureEnabled(bitMask, feature);
    } else {
      SetBitMask(cluster, container_id, ns);
      uint64_t bitMask = *containerFeatureBitMask_[container_id];
      return IsFeatureEnabled(bitMask, feature);
    }
  }
}

}  // namespace collector::runtime_control
