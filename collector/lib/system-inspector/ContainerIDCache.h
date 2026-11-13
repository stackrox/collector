#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>

class sinsp;
class sinsp_evt;
class sinsp_threadinfo;

namespace collector::system_inspector {

// Caches container IDs extracted from thread cgroups, keyed by thread ID and
// validated by clone and last-exec timestamps. Only non-empty extractions are
// cached: cgroups can be populated after a thread is first seen (e.g. a dropped
// clone event, with the cgroups arriving on the next execve), so empty lookups
// recompute until a container ID is found.
class ContainerIDCache final {
 public:
  void Initialize(sinsp& inspector);
  void Prune(sinsp& inspector, uint64_t now_us);
  std::string Get(const sinsp_threadinfo& tinfo) const;
  std::size_t Size() const;
  // Returns whether a thread has a container ID, populating the cache on a miss.
  bool Exists(const sinsp_threadinfo& tinfo) const;
  // Returns an empty string if the event carries no thread information.
  std::string Get(sinsp_evt& evt) const;

 private:
  struct Entry {
    uint64_t clone_ts;
    // libsinsp refreshes cgroups on exec, which can change the container ID.
    uint64_t last_exec_ts;
    std::string container_id;
  };

  std::string Cache(const sinsp_threadinfo& tinfo) const;

  mutable std::mutex mutex_;
  mutable std::unordered_map<int64_t, Entry> entries_;
  uint64_t last_prune_us_ = 0;
};

}  // namespace collector::system_inspector
