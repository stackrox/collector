#ifndef COLLECTOR_PROFILER_H
#define COLLECTOR_PROFILER_H

#include <string>

#ifdef COLLECTOR_PROFILING
#  include "gperftools/heap-profiler.h"
#  include "gperftools/profiler.h"

namespace collector {
class Profiler {
 public:
  static inline void RegisterCPUThread() { ProfilerRegisterThread(); }
  static inline const char* AllocAndGetHeapProfile() {
    if (!IsHeapProfilerEnabled()) {
      return nullptr;
    }
    return GetHeapProfile();
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
  void RegisterCPUThread() {}
  const char* AllocAndGetHeapProfile() { return nullptr; }
  bool StartCPUProfiler(const std::string& output_dir) { return false; }
  void StopCPUProfiler() {}
  void StartHeapProfiler() {}
  void StopHeapProfiler() {}
  bool IsCPUProfilerSupported() { return false; }
  bool IsCPUProfilerEnabled() { return false; }
  bool IsHeapProfilerSupported() { return false; }
  bool IsHeapProfilerEnabled() { return false; }
};
}  // namespace collector
#endif

#endif  //COLLECTOR_PROFILER_H
