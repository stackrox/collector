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
  Config() {
    for (int i = 0; i < storage::RuntimeFilter_RuntimeFilterFeatures_RuntimeFilterFeatures_ARRAYSIZE; i++) {
      kFeatureFlags_[i] = 1UL << (i + 1);
    }
  }

  static Config& GetOrCreate();

  // returns true when initialized, false for a timeout.
  bool WaitUntilInitialized(unsigned int timeout_ms);

  void Update(const storage::RuntimeFilteringConfiguration& msg);
  bool IsFeatureEnabled(uint64_t bitMask, storage::RuntimeFilter_RuntimeFilterFeatures feature);
  bool IsFeatureEnabled(std::string cluster, std::string ns, std::string container_id, storage::RuntimeFilter_RuntimeFilterFeatures feature);
  bool IsFeatureEnabled(std::string cluster, std::string ns, storage::RuntimeFilter_RuntimeFilterFeatures feature);
  void ConfigMessageToConfig(const storage::RuntimeFilteringConfiguration& msg);

 private:
  std::mutex mutex_;
  std::condition_variable condition_;
  std::optional<storage::RuntimeFilteringConfiguration> config_message_;
  uint64_t kFeatureFlags_[storage::RuntimeFilter_RuntimeFilterFeatures_RuntimeFilterFeatures_ARRAYSIZE];
  UnorderedMap<std::string, uint64_t> namespaceFeatureBitMask_;
  UnorderedMap<std::string, uint64_t*> containerFeatureBitMask_;
  uint64_t SetFeatureBitMask(uint64_t bitMask, bool enabled, storage::RuntimeFilter_RuntimeFilterFeatures feature);
  void SetBitMask(std::string cluster, std::string ns);
  void SetBitMask(std::string cluster, std::string container_id, std::string ns);
  UnorderedMap<storage::RuntimeFilter_RuntimeFilterFeatures, bool> default_status_map_;
  UnorderedMap<storage::RuntimeFilter_RuntimeFilterFeatures, std::vector<storage::RuntimeFilter_RuntimeFilterRule> > config_by_feature_;
  UnorderedMap<std::string, storage::ResourceCollection> rcMap_;
};

}  // namespace collector::runtime_control

#endif
