#include "ContainerIDCache.h"

#include <unordered_set>

#include <libsinsp/sinsp.h>
#include <libsinsp/thread_manager.h>
#include <libsinsp/threadinfo.h>

#include "CollectorStats.h"
#include "Utility.h"

namespace collector::system_inspector {

std::string ContainerIDCache::Cache(const sinsp_threadinfo& tinfo) const {
  std::string container_id;
  for (const auto& cgroup : tinfo.cgroups()) {
    if (const auto id = ExtractContainerIDFromCgroup(cgroup.second)) {
      container_id = *id;
      break;
    }
  }

  // Only cache confirmed container IDs. An empty extraction can be transient
  // (cgroups are populated when libsinsp (re)creates or refreshes the thread
  // info, e.g. from an event that permits a /proc query or on clone/execve
  // exit parsing), so empty lookups must recompute. A thread's container ID
  // otherwise never changes during its lifetime.
  if (!container_id.empty()) {
    std::lock_guard<std::mutex> lock(mutex_);
    // A reused tid gets a new clone timestamp, so a stale entry can never
    // validate as a hit; it misses and recomputes.
    entries_[tinfo.m_tid] = {tinfo.m_clone_ts, container_id};
  } else {
    // Separates threads without a container ID (expected, e.g. host
    // processes) from genuine cache misses, so a hit ratio is computable.
    COUNTER_INC(CollectorStats::container_id_cache_empty_extractions);
  }
  return container_id;
}

void ContainerIDCache::Initialize(sinsp& inspector) {
  inspector.m_thread_manager->get_threads()->loop([this](sinsp_threadinfo& tinfo) {
    Cache(tinfo);
    return true;
  });
}

// Prune has a single caller, the event loop under the service's libsinsp
// mutex, so last_prune_us_ needs no atomic.
void ContainerIDCache::Prune(sinsp& inspector, uint64_t now_us) {
  if (now_us - last_prune_us_ < 60'000'000) {
    return;
  }
  last_prune_us_ = now_us;

  std::unordered_set<int64_t> live_tids;
  inspector.m_thread_manager->get_threads()->loop([&live_tids](sinsp_threadinfo& tinfo) {
    live_tids.insert(tinfo.m_tid);
    return true;
  });

  std::lock_guard<std::mutex> lock(mutex_);
  for (auto it = entries_.begin(); it != entries_.end();) {
    if (live_tids.count(it->first) == 0) {
      it = entries_.erase(it);
    } else {
      ++it;
    }
  }
}

std::string ContainerIDCache::Get(const sinsp_threadinfo& tinfo) const {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto entry = entries_.find(tinfo.m_tid);
    if (entry != entries_.end() && entry->second.clone_ts == tinfo.m_clone_ts) {
      COUNTER_INC(CollectorStats::container_id_cache_hits);
      return entry->second.container_id;
    }
    COUNTER_INC(CollectorStats::container_id_cache_misses);
  }

  return Cache(tinfo);
}

bool ContainerIDCache::Exists(const sinsp_threadinfo& tinfo) const {
  return !Get(tinfo).empty();
}

std::string ContainerIDCache::Get(sinsp_evt& evt) const {
  auto* tinfo = evt.get_thread_info();
  if (tinfo == nullptr) {
    return {};
  }
  return Get(*tinfo);
}

}  // namespace collector::system_inspector
