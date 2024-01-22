#ifndef _CONTAINER_ENGINE_H_
#define _CONTAINER_ENGINE_H_

#include "container_engine/container_cache_interface.h"
#include "container_engine/container_engine_base.h"
#include "threadinfo.h"

namespace collector {
class ContainerEngine : public libsinsp::container_engine::container_engine_base {
 public:
  ContainerEngine(libsinsp::container_engine::container_cache_interface& cache) : libsinsp::container_engine::container_engine_base(cache) {}

  bool resolve(sinsp_threadinfo* tinfo, bool query_os_for_missing_info) override {
    for (const auto& cgroup : tinfo->cgroups()) {
      auto container_id = ExtractContainerIDFromCgroup(cgroup.second);

      if (container_id) {
        tinfo->m_container_id = *container_id;
        return true;
      }
    }

    return false;
  }
};
}  // namespace collector

#endif  // _CONTAINER_ENGINE_H_
