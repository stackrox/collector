#ifndef COLLECTOR_MEMORYSTATS_H
#define COLLECTOR_MEMORYSTATS_H

#ifdef COLLECTOR_PROFILING

#include "gperftools/malloc_extension.h"

namespace collector {

class MemoryStats {
 public:
  // Bytes of allocated memory in use.
  static inline size_t AllocatedSize() {
    size_t n = 0;
    ::MallocExtension::instance()->GetNumericProperty("generic.current_allocated_bytes", &n);
    return n;
  }

  // Bytes of system memory reserved, including unallocated memory.
  static inline size_t HeapSize() {
    size_t n = 0;
    ::MallocExtension::instance()->GetNumericProperty("generic.heap_size", &n);
    return n;
  }

  // Bytes of resident memory, or total number of physical bytes in use.
  static inline size_t PhysicalSize() {
    size_t n = 0;
    ::MallocExtension::instance()->GetNumericProperty("generic.total_physical_bytes", &n);
    return n;
  }
};

} // namespace collector

#else
namespace collector {
class MemoryStats {
 public:
  static inline size_t AllocatedSize() { return 0; }
  static inline size_t HeapSize() { return 0; }
  static inline size_t PhysicalSize() { return 0; }
};
} // namespace collector

#endif // COLLECTOR_PROFILING

#endif  // COLLECTOR_MEMORYSTATS_H
