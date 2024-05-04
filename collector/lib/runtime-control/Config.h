#ifndef _RUNTIME_CONTROL_CONFIG_H_
#define _RUNTIME_CONTROL_CONFIG_H_

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>

#include <storage/runtime_filters.pb.h>

#include "Hash.h"

#include "../ResourceSelector.h"

namespace collector::runtime_control {

class Config {
 public:
  static Config& GetOrCreate();

  void Update(const storage::RuntimeFilteringConfiguration& msg);
  bool IsFeatureEnabled(uint64_t bitMask, storage::RuntimeFilterFeatures feature);
  bool IsFeatureEnabled(const std::string& cluster, const std::string& ns, const std::string& container_id, storage::RuntimeFilterFeatures feature);
  bool IsFeatureEnabled(const std::string& cluster, const std::string& ns, storage::RuntimeFilterFeatures feature);
  void ConfigMessageToConfig(const storage::RuntimeFilteringConfiguration& msg);

 private:
  Config() {
    for (int i = 0; i < storage::RuntimeFilterFeatures_ARRAYSIZE; i++) {
      kFeatureFlags_[i] = 1UL << (i + 1);
    }

    storage::RuntimeFilter processRuntime;
    storage::RuntimeFilterFeatures processFeature = storage::RuntimeFilterFeatures::PROCESSES;
    processRuntime.set_feature(processFeature);
    processRuntime.set_default_status("on");

    auto runtimeFilters = config_message_.mutable_runtime_filters();
    runtimeFilters->Add()->CopyFrom(processRuntime);
    ConfigMessageToConfig(config_message_);
  }
  std::mutex mutex_;
  std::condition_variable condition_;
  storage::RuntimeFilteringConfiguration config_message_;
  uint64_t kFeatureFlags_[storage::RuntimeFilterFeatures_ARRAYSIZE];
  UnorderedMap<std::string, uint64_t> namespaceFeatureBitMask_;
  UnorderedMap<std::string, uint64_t*> containerFeatureBitMask_;
  uint64_t SetFeatureBitMask(uint64_t bitMask, bool enabled, storage::RuntimeFilterFeatures feature);
  void SetBitMask(const std::string& cluster, const std::string& ns);
  void SetBitMask(const std::string& cluster, const std::string& container_id, const std::string& ns);
  void SetBitMasksForNamespaces(const std::string& cluster);
  UnorderedMap<storage::RuntimeFilterFeatures, bool> default_status_map_;
  UnorderedMap<storage::RuntimeFilterFeatures, std::vector<storage::RuntimeFilter_RuntimeFilterRule> > config_by_feature_;
  UnorderedMap<std::string, storage::ResourceCollection> rcMap_;
};

}  // namespace collector::runtime_control

#endif
