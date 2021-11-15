#ifndef COLLECTOR_HOSTHEURISTICS_H
#define COLLECTOR_HOSTHEURISTICS_H

#include "CollectorConfig.h"
#include "HostInfo.h"

namespace collector {

class Heuristic {
 public:
  virtual void Process(HostInfo& host, CollectorConfig* config) {}
};

extern const std::vector<Heuristic*> g_host_heuristics;

}  // namespace collector

#endif  // COLLECTOR_HOSTHEURISTICS_H
