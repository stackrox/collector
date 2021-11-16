#ifndef COLLECTOR_HOSTHEURISTICS_H
#define COLLECTOR_HOSTHEURISTICS_H

#include "CollectorConfig.h"
#include "HostConfig.h"
#include "HostInfo.h"

namespace collector {

class Heuristic {
 public:
  virtual void Process(HostInfo& host, const CollectorConfig& config, HostConfig* hconfig) {}
};

// Processes all known heuristics, allowing them to populate host_config
// based on their results.
void ProcessHostHeuristics(const CollectorConfig& config, HostConfig* host_config);

}  // namespace collector

#endif  // COLLECTOR_HOSTHEURISTICS_H
