#pragma once

#include "CollectorConfig.h"
#include "HostConfig.h"
#include "HostInfo.h"

namespace collector {

// Processes all known heuristics, constructing an appropriate HostConfig
// that allows collector to operate effectively on this platform.
HostConfig ProcessHostHeuristics(const CollectorConfig& config);

}  // namespace collector
