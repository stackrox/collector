#include "ContainerMetadata.h"

#include <libsinsp/sinsp.h>

#include "Logging.h"
#include "system-inspector/EventExtractor.h"

namespace collector {

ContainerMetadata::ContainerMetadata(sinsp* inspector) : event_extractor_(std::make_unique<system_inspector::EventExtractor>()), inspector_(inspector) {
  event_extractor_->Init(inspector);
}

std::string ContainerMetadata::GetNamespace(sinsp_evt* event) {
  const char* ns = event_extractor_->get_k8s_namespace(event);
  return ns != nullptr ? ns : "";
}

std::string ContainerMetadata::GetNamespace(const std::string& container_id) {
  return GetContainerLabel(container_id, "io.kubernetes.pod.namespace");
}

std::string ContainerMetadata::GetContainerLabel(const std::string& container_id, const std::string& label) {
  // Container labels are no longer available through the sinsp API.
  // The container plugin provides container metadata via filter fields
  // (e.g., container.label) but not through a programmatic lookup API.
  CLOG_THROTTLED(DEBUG, std::chrono::seconds(300))
      << "Container label lookup by container ID is not supported: "
      << "container_id=" << container_id << " label=" << label;
  return "";
}

}  // namespace collector