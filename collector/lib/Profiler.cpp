#include "Profiler.h"

#include <string>

#ifdef COLLECTOR_PROFILING

#  include "gperftools/heap-profiler.h"
#  include "gperftools/profiler.h"

namespace collector {

// CPU profiler

bool Profiler::IsCPUProfilerSupported() { return true; }

bool Profiler::IsCPUProfilerEnabled() { return ProfilingIsEnabledForAllThreads(); }

bool Profiler::StartCPUProfiler(const std::string& output_dir) {
  return ProfilerStart(output_dir.c_str());
}

void Profiler::StopCPUProfiler() {
  ProfilerFlush();
  ProfilerStop();
}

void Profiler::RegisterCPUThread() { ProfilerRegisterThread(); }

// Heap profiler

bool Profiler::IsHeapProfilerSupported() { return true; }

bool Profiler::IsHeapProfilerEnabled() { return IsHeapProfilerRunning(); }

void Profiler::StartHeapProfiler() { HeapProfilerStart(""); }

const char* Profiler::AllocAndGetHeapProfile() {
  if (!IsHeapProfilerEnabled()) {
    return nullptr;
  }
  return GetHeapProfile();
}

void Profiler::StopHeapProfiler() {
  if (!IsHeapProfilerEnabled()) {
    return;
  }
  HeapProfilerStop();
}
}  // namespace collector

#else

namespace collector {

bool Profiler::IsCPUProfilerSupported() { return false; }
bool Profiler::IsCPUProfilerEnabled() { return false; }
bool Profiler::StartCPUProfiler(const std::string& output_dir) { return false; }
void Profiler::StopCPUProfiler() {}
void Profiler::RegisterCPUThread() {}
bool Profiler::IsHeapProfilerSupported() { return false; }
bool Profiler::IsHeapProfilerEnabled() { return false; }
void Profiler::StartHeapProfiler() {}
const char* Profiler::AllocAndGetHeapProfile() { return nullptr; }
void Profiler::StopHeapProfiler() {}

}  // namespace collector

#endif
