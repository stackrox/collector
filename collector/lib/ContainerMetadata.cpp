#include "ContainerMetadata.h"

#include <libsinsp/sinsp.h>

#include "system-inspector/EventExtractor.h"

namespace collector {

ContainerMetadata::ContainerMetadata(sinsp* inspector) : event_extractor_(std::make_unique<system_inspector::EventExtractor>()), inspector_(inspector) {
  event_extractor_->Init(inspector);
}

std::string ContainerMetadata::GetNamespace(sinsp_evt* event) {
  return "";
}

std::string ContainerMetadata::GetNamespace(const std::string& container_id) {
  return GetContainerLabel(container_id, "io.kubernetes.pod.namespace");
}

std::string ContainerMetadata::GetContainerLabel(const std::string& container_id, const std::string& label) {
  return "";
}

}  // namespace collector
