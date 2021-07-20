#ifndef COLLECTOR_NODEMETRICS_H
#define COLLECTOR_NODEMETRICS_H

#include <unistd.h>

#include "Containers.h"
#include "FileSystem.h"
#include "Utility.h"

namespace collector {

class NodeMetrics {
 public:
  inline static size_t NumProcessors() {
    return sysconf(_SC_NPROCESSORS_ONLN);
  }
};

class CPUThrottleMetrics {
 public:
  struct cpu_throttle_metric_t {
    // nr_periods – number of periods that any thread in the cgroup was runnable
    size_t nr_periods;
    // nr_throttled – number of runnable periods in which the application used its entire quota and was throttled
    size_t nr_throttled;
    // throttled_time – sum total amount of time individual threads within the cgroup were throttled
    size_t throttled_time;
  };

  inline static bool ReadStatFile(cpu_throttle_metric_t* m) {
    if (!m) {
      return false;
    }
    FileHandle cpu_stat_file(FDHandle(open("/sys/fs/cgroup/cpu/cpu.stat", O_RDONLY)), "r");
    if (!cpu_stat_file.valid()) {
      return false;
    }
    char line[512];
    std::map<std::string, size_t*> fields = {
        {"nr_periods", &m->nr_periods},
        {"nr_throttled", &m->nr_throttled},
        {"throttled_time", &m->throttled_time}};
    size_t read_count = 0;
    while (std::fgets(line, sizeof(line), cpu_stat_file)) {
      std::stringstream ss_line(line);
      std::string k, v;
      if (ss_line >> k >> v) {
        size_t** value_ptr = Lookup(fields, k);
        if (!value_ptr && !*value_ptr) {
          continue;
        }
        **value_ptr = stoul(v);
        *value_ptr = nullptr;
        ++read_count;
      }
    }
    return read_count == fields.size();
  }
};

}  // namespace collector

#endif  //COLLECTOR_NODEMETRICS_H
