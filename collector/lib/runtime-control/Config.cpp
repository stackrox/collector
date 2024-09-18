#include <runtime-control/Config.h>
#include <runtime-control/NamespaceSelector.h>

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
  auto process_config = msg.process_config();
  bool process_enabled = process_config.enabled();
  CLOG(INFO) << "process_enabled= " << process_enabled;

  condition_.notify_all();
}

bool Config::IsFeatureEnabled(uint64_t bitMask, int feature) {
  return (bitMask & kFeatureFlags_[feature]) != 0;
}

void Config::SetBitMask(const std::string& ns) {
  if (config_message_.has_value()) {
    uint64_t bitMask = 0;
    const auto& cluster_scope_config = config_message_.value().cluster_scope_config();
    const auto& namespace_scope_config = config_message_.value().namespace_scope_config();

    for (const auto& config : cluster_scope_config) {
      auto& default_instance = config.default_instance();
      if (default_instance.feature_case() == storage::CollectorFeature::FeatureCase::kNetworkConnections) {
        auto network_config = reinterpret_cast<const storage::NetworkConnectionConfig*>(&default_instance);
        bool enabled = network_config->aggregate_external();
        bitMask = enabled ? (bitMask | 1) : (bitMask & ~1);
      }
    }

    for (const auto& config : namespace_scope_config) {
      auto& feature = config.feature();
      auto& default_instance = feature.default_instance();
      if (default_instance.feature_case() == storage::CollectorFeature::FeatureCase::kNetworkConnections) {
        auto network_config = reinterpret_cast<const storage::NetworkConnectionConfig*>(&default_instance);
        bool enabled = network_config->aggregate_external();
        auto nsSelection = config.namespace_selection();

        bool inNs = NamespaceSelector::IsNamespaceInSelection(nsSelection, ns);
        if (inNs) {
          bitMask = enabled ? (bitMask | 1) : (bitMask & ~1);
        }
        CLOG(INFO) << "NeworkConnections";
      }
    }

    namespaceFeatureBitMask_[ns] = bitMask;
  }
}

void Config::SetBitMasksForNamespaces() {
  for (auto it = namespaceFeatureBitMask_.begin(); it != namespaceFeatureBitMask_.end();) {
    auto& pair = *it;
    std::string ns = pair.first;
    SetBitMask(ns);
  }
}

void Config::SetBitMask(std::string container_id, std::string ns) {
  SetBitMask(ns);
  containerFeatureBitMask_[container_id] = &namespaceFeatureBitMask_[ns];
  CLOG(INFO) << "namespaceFeatureBitMask_[ns]= " << namespaceFeatureBitMask_[ns];
  CLOG(INFO) << "*containerFeatureBitMask_[container_id]= " << *containerFeatureBitMask_[container_id];
}

// Should probably just take in the container_id, check the containerFeatureBitMask_ map, and only figure out the namespace if the container is not in the map.
// For now this is easier for testing purposes.
bool Config::IsFeatureEnabled(std::string ns, std::string container_id, int feature) {
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
      SetBitMask(container_id, ns);
      uint64_t bitMask = *containerFeatureBitMask_[container_id];
      return IsFeatureEnabled(bitMask, feature);
    }
  }
}

}  // namespace collector::runtime_control
