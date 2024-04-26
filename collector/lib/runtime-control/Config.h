#ifndef _RUNTIME_CONTROL_CONFIG_H_
#define _RUNTIME_CONTROL_CONFIG_H_

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>

#include <storage/runtime_filters.pb.h>

#include "Hash.h"

namespace collector::runtime_control {

class Config {
 public:
  static Config& GetOrCreate();

  // returns true when initialized, false for a timeout.
  bool WaitUntilInitialized(unsigned int timeout_ms);

  void Update(const storage::RuntimeFilteringConfiguration& msg);
  static bool IsProcessEnabled(uint64_t bitMask);
  bool IsProcessEnabled(std::string cluster, std::string ns, std::string container_id);
  bool IsProcessEnabled(std::string cluster, std::string ns);

 private:
  std::mutex mutex_;
  std::condition_variable condition_;
  std::optional<storage::RuntimeFilteringConfiguration> config_message_;
  UnorderedMap<std::string, uint64_t*> namespaceFeatureBitMask_;
  UnorderedMap<std::string, uint64_t*> containerFeatureBitMask_;
  void SetProcessBitMask(bool enabled, std::string container, std::string ns);
};

}  // namespace collector::runtime_control

#endif
