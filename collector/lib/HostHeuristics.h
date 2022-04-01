#ifndef COLLECTOR_HOSTHEURISTICS_H
#define COLLECTOR_HOSTHEURISTICS_H

#include "CollectorConfig.h"
#include "HostConfig.h"
#include "HostInfo.h"

namespace collector {

// Processes all known heuristics, constructing an appropriate HostConfig
// that allows collector to operate effectively on this platform.
HostConfig ProcessHostHeuristics(const CollectorConfig& config);

}  // namespace collector

#endif  // COLLECTOR_HOSTHEURISTICS_H
