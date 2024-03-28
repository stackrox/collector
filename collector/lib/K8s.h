#ifndef _K8S_H_
#define _K8S_H_

#include <sstream>
#include <string_view>

#include "system-inspector/EventExtractor.h"

namespace collector {

class K8s {
 public:
  K8s(sinsp* inspector) : inspector_(inspector) {
    event_extractor_.Init(inspector);
  }

  inline std::string_view GetNamespace(sinsp_evt* event) {
    const char* ns = event_extractor_.get_k8s_namespace(event);
    return ns != nullptr ? ns : "";
  }

  inline std::string_view GetNamespace(const std::string& container_id) {
    return GetContainerLabel(container_id, "io.kubernetes.pod.namespace");
  }

  std::string GetContainerLabels(const std::string& container_id) {
    const auto container = inspector_->m_container_manager.get_container(container_id);
    if (container == nullptr) {
      return "";
    }

    std::stringstream ss;

    for (const auto& [key, value] : container->m_labels) {
      ss << key << ":" << value << ",";
    }

    return ss.str();
  }

  inline std::string_view GetContainerLabel(const std::string& container_id, const std::string& label) {
    const auto container = inspector_->m_container_manager.get_container(container_id);
    if (container == nullptr || container->m_labels.count(label) == 0) {
      return "";
    }
    return container->m_labels.at(label);
  }

 private:
  system_inspector::EventExtractor event_extractor_;
  sinsp* inspector_;
};

}  // namespace collector

#endif  // _K8S_H_
