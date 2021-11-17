#ifndef COLLECTOR_HOSTHEURISTICS_H
#define COLLECTOR_HOSTHEURISTICS_H

#include "CollectorConfig.h"
#include "HostConfig.h"
#include "HostInfo.h"

namespace collector {

class Heuristic {
 public:
  // Process the given HostInfo and CollectorConfig to adjust HostConfig as necessary.
  // It is intended that any number of Heuristics may be applied to the configs,
  // to allow overriding of specific configuration options based on the platform.
  // Note: non-const reference to HostInfo due to its lazy-initialization.
  virtual void Process(HostInfo& host, const CollectorConfig& config, HostConfig* hconfig) const {}
};

// Processes all known heuristics, allowing them to populate host_config
// based on their results.
void ProcessHostHeuristics(const CollectorConfig& config, HostConfig* host_config);

}  // namespace collector

#endif  // COLLECTOR_HOSTHEURISTICS_H
