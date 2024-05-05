#include <runtime-control/Config.h>

#include "ContainerMetadata.h"
#include "Logging.h"
#include "system-inspector/EventExtractor.h"

namespace collector::runtime_control {

Config& Config::GetInstance() {
  static Config config;

  return config;
}

void Config::Update(const storage::RuntimeFilteringConfiguration& msg) {
  std::unique_lock<std::mutex> lock(mutex_);

  config_message_ = std::move(msg);
  ConfigMessageToConfig(msg);
  std::string cluster = "cluster-1";  // Cluster is always the same, so should probably not use it anywhere. Using it but hardcoding it for now.
  SetBitMasksForNamespaces(cluster);

  condition_.notify_all();
}

void Config::ConfigMessageToConfig(const storage::RuntimeFilteringConfiguration& msg) {
  for (storage::RuntimeFilter runtimeConfig : msg.runtime_filters()) {
    CLOG(INFO) << "runtimeConfig.feature()= " << runtimeConfig.feature();
    storage::RuntimeFilterFeatures feature = runtimeConfig.feature();
    if (feature < storage::RuntimeFilterFeatures_ARRAYSIZE) {
      if (runtimeConfig.default_status().compare("on") == 0) {
        default_status_[feature] = true;
        CLOG(INFO) << "Setting default to true";
      } else {
        default_status_[feature] = false;
        CLOG(INFO) << "Setting default to false";
      }

      config_by_feature_[feature].clear();
      for (auto runtimeConfigRule : runtimeConfig.rules()) {
        config_by_feature_[feature].push_back(runtimeConfigRule);
      }
    }
  }

  for (storage::ResourceCollection rc : msg.resource_collections()) {
    rcMap_[rc.id()] = rc;
    CLOG(INFO) << "Adding rc.id()= " << rc.id();
  }
}

inline bool Config::IsFeatureEnabled(uint64_t bitMask, storage::RuntimeFilterFeatures feature) {
  return (bitMask & kFeatureFlags_[feature]) != 0;
}

inline bool Config::IsFeatureEnabled(const std::string& cluster, const std::string& ns, storage::RuntimeFilterFeatures feature) {
  if (feature < storage::RuntimeFilterFeatures_ARRAYSIZE) {
    return ResourceSelector::IsFeatureEnabledForClusterAndNamespace(config_by_feature_[feature], rcMap_, default_status_[feature], cluster, ns);
  } else {
    return false;
  }
}

inline void Config::SetFeatureBitMask(uint64_t& bitMask, bool enabled, storage::RuntimeFilterFeatures feature) {
  bitMask = enabled ? (bitMask | kFeatureFlags_[feature]) : (bitMask & ~kFeatureFlags_[feature]);
}

void Config::SetBitMask(const std::string& cluster, const std::string& ns) {
  uint64_t bitMask = 0;
  for (int i = 0; i < storage::RuntimeFilterFeatures_ARRAYSIZE; i++) {
    CLOG(INFO) << "i= " << i;
    storage::RuntimeFilterFeatures feature = static_cast<storage::RuntimeFilterFeatures>(i);
    bool enabled = IsFeatureEnabled(cluster, ns, feature);
    CLOG(INFO) << "enabled= " << enabled;
    SetFeatureBitMask(bitMask, enabled, feature);
  }
  CLOG(INFO) << "ns= " << ns << " bitMask= " << bitMask;
  namespaceFeatureBitMask_[ns] = bitMask;
}

void Config::SetBitMasksForNamespaces(const std::string& cluster) {
  for (auto it = namespaceFeatureBitMask_.begin(); it != namespaceFeatureBitMask_.end(); ++it) {
    auto& pair = *it;
    std::string ns = pair.first;
    SetBitMask(cluster, ns);
  }
}

void Config::SetBitMask(const std::string& cluster, const std::string& container_id, const std::string& ns) {
  SetBitMask(cluster, ns);
  containerFeatureBitMask_[container_id] = &namespaceFeatureBitMask_[ns];
  CLOG(INFO) << "namespaceFeatureBitMask_[ns]= " << namespaceFeatureBitMask_[ns];
  CLOG(INFO) << "*containerFeatureBitMask_[container_id]= " << *containerFeatureBitMask_[container_id];
}

// Should probably just take in the container_id, check the containerFeatureBitMask_ map, and only figure out the namespace if the container is not in the map.
// For now this is easier for testing purposes.
bool Config::IsFeatureEnabled(const std::string& cluster, const std::string& ns, const std::string& container_id, storage::RuntimeFilterFeatures feature) {
  auto bitMaskPair = containerFeatureBitMask_.find(container_id);
  if (bitMaskPair != containerFeatureBitMask_.end()) {
    CLOG(INFO) << "Found in container map"
               << " container_id= " << container_id << " ns= " << ns;
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

// Probably won't keep both versions. Probably won't pass in container_metadata, but add it to the class later on.
bool Config::IsFeatureEnabled(const std::string& cluster, sinsp_evt* event, ContainerMetadata& container_metadata, const std::string& container_id, storage::RuntimeFilterFeatures feature) {
  auto bitMaskPair = containerFeatureBitMask_.find(container_id);
  if (bitMaskPair != containerFeatureBitMask_.end()) {
    CLOG(INFO) << "Found in container map"
               << " container_id= " << container_id;
    uint64_t* bitMask = bitMaskPair->second;
    return IsFeatureEnabled(*bitMask, feature);
  } else {
    std::string ns = container_metadata.GetNamespace(event);
    CLOG(INFO) << "ns= " << ns;
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
