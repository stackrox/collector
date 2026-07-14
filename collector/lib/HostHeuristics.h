#pragma once

#include "CollectorConfig.h"
#include "HostConfig.h"
#include "HostInfo.h"

namespace collector {

/// Apply platform-specific workarounds to the collector configuration.
/// Each quirk (missing BTF, Docker Desktop, RHEL/ppc64le bugs, etc.) is
/// a Heuristic subclass in HostHeuristics.cpp, run once at startup. Add
/// new platform quirks as additional subclasses there.
HostConfig ProcessHostHeuristics(const CollectorConfig& config);

}  // namespace collector
