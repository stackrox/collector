//
// Created by Robby Cochran on 6/7/21.
//

#ifndef COLLECTOR_PROFILER_H
#define COLLECTOR_PROFILER_H

#include <string>
namespace collector {

class ProfilerHandler {
 public:

};

class Profiler {
 public:
  static bool StartCPUProfiler(const std::string& output_dir);
  static void StartHeapProfiler(const std::string& output_dir);
  static void StopCPUProfiler();
  static void StopHeapProfiler();
  static bool IsCPUProfilerEnabled();
  static bool IsHeapProfilerEnabled();
};

} // namespace collector

#endif  //COLLECTOR_PROFILER_H
