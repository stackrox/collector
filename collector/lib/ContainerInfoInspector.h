#ifndef _CONTAINER_INFO_INSPECTOR_
#define _CONTAINER_INFO_INSPECTOR_

#include <CivetServer.h>
#include <civetweb.h>
#include <memory>
#include <string>
#include <unordered_map>

#include "IntrospectionEndpoint.h"
#include "K8s.h"
#include "json/writer.h"

namespace collector {

using QueryParams = std::unordered_map<std::string, std::string>;

class ContainerInfoInspector : public IntrospectionEndpoint {
 public:
  static const char* kBaseRoute;

  ContainerInfoInspector(const std::shared_ptr<K8s>& k8s_inspector) : k8s_inspector_(k8s_inspector) {}

  // implementation of CivetHandler
  bool handleGet(CivetServer* server, struct mg_connection* conn) override;

 private:
  std::shared_ptr<K8s> k8s_inspector_;
  Json::FastWriter writer_;
};

}  // namespace collector

#endif  //_CONTAINER_INFO_INSPECTOR_
