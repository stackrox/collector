#ifndef COLLECTOR_GETKERNELOBJECT_H
#define COLLECTOR_GETKERNELOBJECT_H

#include <string>

#include <json/json.h>

namespace collector {

bool GetKernelObject(const std::string& hostname, const Json::Value& tls_config, const std::string& kernel_module, const std::string& module_path, bool verbose);

}  // namespace collector
#endif  // COLLECTOR_GETKERNELOBJECT_H
