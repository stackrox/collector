//
// Created by Robby Cochran on 6/16/21.
//

#include "Profiler.h"
#include "gperftools/heap-profiler.h"
#include "gperftools/profiler.h"


namespace collector {

  bool Profiler::StartCPUProfiler(const std::string& output_dir) {
    return ProfilerStart(output_dir.c_str());
  }
  void Profiler::StopCPUProfiler() {
    ProfilerStop();
  }

  void Profiler::StartHeapProfiler(const std::string& output_dir) {
    HeapProfilerStart(output_dir.c_str());
  }
  void Profiler::StopHeapProfiler() {
    if (IsHeapProfilerEnabled()){
      HeapProfilerDump("stop");
      HeapProfilerStop();
    }
  }
  bool Profiler::IsCPUProfilerEnabled() {
    return ProfilingIsEnabledForAllThreads() != 0;
  }
  bool Profiler::IsHeapProfilerEnabled() {
    return IsHeapProfilerRunning() != 0;
  }

} // namespace collector


