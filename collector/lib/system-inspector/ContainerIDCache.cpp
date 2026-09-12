#include "ContainerIDCache.h"

#include <unordered_set>

#include <libsinsp/sinsp.h>
#include <libsinsp/thread_manager.h>
#include <libsinsp/threadinfo.h>

#include "Utility.h"

namespace collector::system_inspector {

void ContainerIDCache::Cache(const sinsp_threadinfo& tinfo) {
  std::string container_id;
  for (const auto& cgroup : tinfo.cgroups()) {
    if (const auto id = ExtractContainerIDFromCgroup(cgroup.second)) {
      container_id = *id;
      break;
    }
  }

  std::lock_guard<std::mutex> lock(mutex_);
  entries_[tinfo.m_tid] = {tinfo.m_clone_ts, std::move(container_id)};
}

void ContainerIDCache::Initialise(sinsp& inspector) {
  inspector.m_thread_manager->get_threads()->loop([this](sinsp_threadinfo& tinfo) {
    Cache(tinfo);
    return true;
  });
}

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
  std::lock_guard<std::mutex> lock(mutex_);
  const auto entry = entries_.find(tinfo.m_tid);
  if (entry == entries_.end() || entry->second.clone_ts != tinfo.m_clone_ts) {
    return {};
  }
  return entry->second.container_id;
}

void ContainerIDCache::on_clone(sinsp_evt*, sinsp_threadinfo* newtinfo, int64_t) {
  if (newtinfo != nullptr) {
    Cache(*newtinfo);
  }
}

void ContainerIDCache::on_execve(sinsp_evt* evt) {
  if (evt != nullptr && evt->get_thread_info() != nullptr) {
    Cache(*evt->get_thread_info());
  }
}

}  // namespace collector::system_inspector
