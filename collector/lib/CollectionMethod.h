#ifndef COLLECTION_METHOD_H
#define COLLECTION_METHOD_H

#include <cstdint>
#include <ostream>
#include <string_view>

namespace collector {
enum class CollectionMethod : uint8_t {
  EBPF = 0,
  CORE_BPF,
};

std::ostream& operator<<(std::ostream& os, CollectionMethod method);

const char* CollectionMethodName(CollectionMethod method);
CollectionMethod ParseCollectionMethod(std::string_view method);

}  // namespace collector

#endif  // COLLECTION_METHOD_H
