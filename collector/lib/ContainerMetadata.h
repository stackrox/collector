#ifndef _CONTAINER_METADATA_H_
#define _CONTAINER_METADATA_H_

#include <memory>
#include <string>

// forward declarations
class sinsp;
class sinsp_evt;
namespace collector {
namespace system_inspector {
class EventExtractor;
}
}  // namespace collector

namespace collector {

class ContainerMetadata {
 public:
  ContainerMetadata(sinsp* inspector);

  std::string GetNamespace(sinsp_evt* event);

  std::string GetNamespace(const std::string& container_id);

  std::string GetContainerLabel(const std::string& container_id, const std::string& label);

 private:
  std::unique_ptr<system_inspector::EventExtractor> event_extractor_;
  sinsp* inspector_;
};

}  // namespace collector

#endif  // _CONTAINER_METADATA_H_
