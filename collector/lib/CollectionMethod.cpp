#include "CollectionMethod.h"

#include <sstream>

namespace collector {

std::ostream& operator<<(std::ostream& os, CollectionMethod method) {
  switch (method) {
    case CollectionMethod::EBPF:
      return os << "ebpf";
    case CollectionMethod::CORE_BPF:
      return os << "core_bpf";
    default:
      return os << "unknown(" << static_cast<uint8_t>(method) << ")";
  }
}

std::string CollectionMethodName(CollectionMethod method) {
  std::stringstream ss;
  ss << method;
  return ss.str();
}

}  // namespace collector
