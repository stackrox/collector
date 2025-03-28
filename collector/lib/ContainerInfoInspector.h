#ifndef _CONTAINER_INFO_INSPECTOR_
#define _CONTAINER_INFO_INSPECTOR_

#include <CivetServer.h>
#include <civetweb.h>
#include <memory>
#include <string>
#include <unordered_map>

#include "CivetWrapper.h"
#include "ContainerMetadata.h"
#include "json/writer.h"

namespace collector {

using QueryParams = std::unordered_map<std::string, std::string>;

class ContainerInfoInspector : public CivetWrapper {
 public:
  ContainerInfoInspector(const std::shared_ptr<ContainerMetadata>& cmi) : container_metadata_inspector_(cmi) {}

  // implementation of CivetHandler
  bool handleGet(CivetServer* server, struct mg_connection* conn) override;

  const std::string& GetBaseRoute() override {
    return kBaseRoute;
  }

 private:
  static const std::string kBaseRoute;

  std::shared_ptr<ContainerMetadata> container_metadata_inspector_;
  Json::FastWriter writer_;
};

}  // namespace collector

#endif  //_CONTAINER_INFO_INSPECTOR_
