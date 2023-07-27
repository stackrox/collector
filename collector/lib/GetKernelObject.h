#ifndef COLLECTOR_GETKERNELOBJECT_H
#define COLLECTOR_GETKERNELOBJECT_H

#include <string>

#include <json/json.h>

#include "DriverCandidates.h"

namespace collector {

bool GetKernelObject(const std::string& hostname, const Json::Value& tls_config, const DriverCandidate& candidate, bool verbose);

}  // namespace collector
#endif  // COLLECTOR_GETKERNELOBJECT_H
