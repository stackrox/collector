#include "ContainerMetadata.h"

#include <libsinsp/sinsp.h>

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
  auto containers = inspector_->m_container_manager.get_containers();
  const auto& container = containers->find(container_id);
  if (container == containers->end()) {
    return "";
  }

  const auto& labels = container->second->m_labels;
  const auto& label_it = labels.find(label);
  if (label_it == labels.end()) {
    return "";
  }

  return label_it->second;
}

}  // namespace collector