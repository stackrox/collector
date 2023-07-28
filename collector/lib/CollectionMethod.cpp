#include "CollectionMethod.h"

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

}  // namespace collector
