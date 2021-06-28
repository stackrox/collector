#ifndef COLLECTOR_PROFILER_H
#define COLLECTOR_PROFILER_H

#include <memory>
#include <string>

struct Free {
  void operator()(const void* p) const { free((void*)p); }
};
using MallocUniquePtr = std::unique_ptr<char, Free>;

#ifdef COLLECTOR_PROFILING
#  include "gperftools/heap-profiler.h"
#  include "gperftools/profiler.h"

namespace collector {
class Profiler {
 public:
  static inline void RegisterCPUThread() { ProfilerRegisterThread(); }
  static inline MallocUniquePtr AllocAndGetHeapProfile() {
    if (!IsHeapProfilerEnabled()) {
      return nullptr;
    }
    return MallocUniquePtr(GetHeapProfile());
  }
  static inline bool StartCPUProfiler(const std::string& output_dir) {
    return ProfilerStart(output_dir.c_str());
  }
  static inline void StopCPUProfiler() {
    ProfilerFlush();
    ProfilerStop();
  }
  static inline void StartHeapProfiler() { HeapProfilerStart(""); }
  static inline void StopHeapProfiler() {
    if (!IsHeapProfilerEnabled()) {
      return;
    }
    HeapProfilerStop();
  }
  static inline bool IsCPUProfilerEnabled() { return ProfilingIsEnabledForAllThreads(); }
  static inline bool IsCPUProfilerSupported() { return true; }
  static inline bool IsHeapProfilerEnabled() { return IsHeapProfilerRunning(); }
  static inline bool IsHeapProfilerSupported() { return true; }
};

}  // namespace collector
#else

namespace collector {
class Profiler {
 public:
  static inline void RegisterCPUThread() {}
  static inline MallocUniquePtr AllocAndGetHeapProfile() { return MallocUniquePtr(nullptr); }
  static inline bool StartCPUProfiler(const std::string& output_dir) { return false; }
  static inline void StopCPUProfiler() {}
  static inline void StartHeapProfiler() {}
  static inline void StopHeapProfiler() {}
  static inline bool IsCPUProfilerSupported() { return false; }
  static inline bool IsCPUProfilerEnabled() { return false; }
  static inline bool IsHeapProfilerSupported() { return false; }
  static inline bool IsHeapProfilerEnabled() { return false; }
};
}  // namespace collector
#endif

#endif  //COLLECTOR_PROFILER_H
