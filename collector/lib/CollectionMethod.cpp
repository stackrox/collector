#include "CollectionMethod.h"

#include <algorithm>

#include <sys/types.h>

#include "log/Logging.h"

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

CollectionMethod ParseCollectionMethod(std::string_view method) {
  // Canonicalize collection method to lowercase, replace '-' with '_'
  std::string cm(method);
  std::transform(cm.begin(), cm.end(), cm.begin(), ::tolower);
  std::replace(cm.begin(), cm.end(), '-', '_');

  if (cm == "ebpf") {
    return CollectionMethod::EBPF;
  }

  if (cm == "core_bpf") {
    return CollectionMethod::CORE_BPF;
  }

  CLOG(WARNING) << "Invalid collection-method (" << cm << "), using CO-RE BPF";
  return CollectionMethod::CORE_BPF;
}
}  // namespace collector
