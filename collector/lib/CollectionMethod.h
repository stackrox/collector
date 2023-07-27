#ifndef COLLECTION_METHOD_H
#define COLLECTION_METHOD_H

#include <ostream>

namespace collector {
enum class CollectionMethod : uint8_t {
  EBPF = 0,
  CORE_BPF,

};

std::ostream& operator<<(std::ostream& os, CollectionMethod method);

}  // namespace collector

#endif  // COLLECTION_METHOD_H
