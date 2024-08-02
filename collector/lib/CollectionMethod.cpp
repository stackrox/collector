#include "CollectionMethod.h"

#include <sstream>

#include <sys/types.h>

#include "Logging.h"

namespace collector {

std::ostream& operator<<(std::ostream& os, CollectionMethod method) {
  return os << CollectionMethodName(method);
}

const char* CollectionMethodName(CollectionMethod method) {
  switch (method) {
    case CollectionMethod::EBPF:
      return "ebpf";
    case CollectionMethod::CORE_BPF:
      return "core_bpf";
    default:
      CLOG(WARNING) << "Unexpected CollectionMethod: " << static_cast<uint8_t>(method);
      return "unknown";
  }
}

}  // namespace collector
