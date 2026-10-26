#include "ContainerIDFilterCheck.h"

#include <cstring>
#include <utility>

#include <libsinsp/filter.h>
#include <libsinsp/sinsp.h>
#include <libsinsp/threadinfo.h>

#include "ContainerIDCache.h"

namespace collector::system_inspector {

std::unique_ptr<sinsp_filter> CompileFilter(
    sinsp& inspector, const ContainerIDCache* container_id_cache, const std::string& expression) {
  sinsp_filter_check_list filter_list;
  filter_list.add_filter_check(inspector.new_generic_filtercheck());
  filter_list.add_filter_check(std::make_unique<ContainerIDFilterCheck>(container_id_cache));
  auto filter_factory = std::make_shared<sinsp_filter_factory>(&inspector, filter_list);
  sinsp_filter_compiler filter_compiler(filter_factory, expression);
  return filter_compiler.compile();
}

namespace {

constexpr char kHostContainerID[] = "host";

const filtercheck_field_info kFields[] = {
    {PT_CHARBUF, EPF_NONE, PF_NA, "container.id", "Cached container ID for the event thread", ""},
};

}  // namespace

ContainerIDFilterCheck::ContainerIDFilterCheck(const ContainerIDCache* container_id_cache)
    : container_id_cache_(container_id_cache) {
  static const filter_check_info info = {
      "container",
      "Container fields",
      "Container fields",
      sizeof(kFields) / sizeof(kFields[0]),
      kFields,
      filter_check_info::FL_NONE,
  };
  m_info = &info;
}

std::unique_ptr<sinsp_filter_check> ContainerIDFilterCheck::allocate_new() {
  return std::make_unique<ContainerIDFilterCheck>(container_id_cache_);
}

uint8_t* ContainerIDFilterCheck::extract_single(sinsp_evt* event, uint32_t* len, bool) {
  *len = 0;
  if (event == nullptr || m_field_id != 0) {
    return nullptr;
  }

  sinsp_threadinfo* tinfo = event->get_thread_info();
  if (tinfo == nullptr) {
    return nullptr;
  }

  result_ = container_id_cache_->Get(*tinfo);
  if (!result_.empty()) {
    *len = result_.size();
    return reinterpret_cast<uint8_t*>(result_.data());
  }

  // Match the bundled container filter: an empty ID identifies the host only
  // when the process is outside a PID namespace.
  if (!tinfo->is_in_pid_namespace()) {
    *len = sizeof(kHostContainerID) - 1;
    return reinterpret_cast<uint8_t*>(const_cast<char*>(kHostContainerID));
  }
  return reinterpret_cast<uint8_t*>(result_.data());
}

}  // namespace collector::system_inspector
