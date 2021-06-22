#ifndef COLLECTOR_PROFILER_H
#define COLLECTOR_PROFILER_H

#include <string>
namespace collector {

class Profiler {
 public:
  static bool StartCPUProfiler(const std::string& output_dir);
  static void StartHeapProfiler();
  static void StopCPUProfiler();
  static void StopHeapProfiler();
  static void RegisterCPUThread();
  static const char* AllocAndGetHeapProfile();
  static bool IsCPUProfilerEnabled();
  static bool IsHeapProfilerEnabled();
  static bool IsCPUProfilerSupported();
  static bool IsHeapProfilerSupported();
};

}  // namespace collector

#endif  //COLLECTOR_PROFILER_H
