#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include <libsinsp/sinsp_filtercheck.h>

namespace collector::system_inspector {

class ContainerIDCache;

class ContainerIDFilterCheck final : public sinsp_filter_check {
 public:
  explicit ContainerIDFilterCheck(const ContainerIDCache* container_id_cache);

  std::unique_ptr<sinsp_filter_check> allocate_new() override;

 protected:
  uint8_t* extract_single(sinsp_evt* event, uint32_t* len, bool sanitize_strings) override;

 private:
  const ContainerIDCache* container_id_cache_;
  std::string result_;
};

}  // namespace collector::system_inspector
