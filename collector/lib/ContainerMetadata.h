#ifndef _CONTAINER_METADATA_H_
#define _CONTAINER_METADATA_H_

#include "system-inspector/EventExtractor.h"

namespace collector {

class ContainerMetadata {
 public:
  ContainerMetadata(sinsp* inspector) : inspector_(inspector) {
    event_extractor_.Init(inspector);
  }

  inline std::string GetNamespace(sinsp_evt* event) {
    const char* ns = event_extractor_.get_k8s_namespace(event);
    return ns != nullptr ? ns : "";
  }

  inline std::string GetNamespace(const std::string& container_id) {
    return GetContainerLabel(container_id, "io.kubernetes.pod.namespace");
  }

  inline std::string GetContainerLabel(const std::string& container_id, const std::string& label) {
    const auto containers = inspector_->m_container_manager.get_containers();
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

 private:
  system_inspector::EventExtractor event_extractor_;
  sinsp* inspector_;
};

}  // namespace collector

#endif  // _CONTAINER_METADATA_H_
