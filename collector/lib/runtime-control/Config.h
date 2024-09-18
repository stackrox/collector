#ifndef _RUNTIME_CONTROL_CONFIG_H_
#define _RUNTIME_CONTROL_CONFIG_H_

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>

#include <internalapi/sensor/collector_iservice.pb.h>

#include "Hash.h"

namespace collector::runtime_control {

class Config {
 public:
  Config() {
    kFeatureFlags_[0] = 1;
  }
  static Config& GetOrCreate();

  // returns true when initialized, false for a timeout.
  bool WaitUntilInitialized(unsigned int timeout_ms);

  void Update(const storage::CollectorConfig& msg);
  bool IsFeatureEnabled(std::string ns, std::string container_id, int feature);

 private:
  std::mutex mutex_;
  std::condition_variable condition_;
  std::optional<storage::CollectorConfig> config_message_;

  bool IsFeatureEnabled(uint64_t bitMask, int feature);
  void SetBitMask(const std::string& ns);
  void SetBitMasksForNamespaces();
  void SetBitMask(std::string container_id, std::string ns);

  UnorderedMap<std::string, uint64_t> namespaceFeatureBitMask_;
  UnorderedMap<std::string, uint64_t*> containerFeatureBitMask_;
  uint64_t kFeatureFlags_[1];
};

}  // namespace collector::runtime_control

#endif
