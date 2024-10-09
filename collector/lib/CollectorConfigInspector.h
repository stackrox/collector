#ifndef COLLECTOR_CONFIG_INSPECTOR_H
#define COLLECTOR_CONFIG_INSPECTOR_H

#include <memory>

#include <json/writer.h>

#include "CollectorConfig.h"
#include "IntrospectionEndpoint.h"

namespace collector {

class CollectorConfigInspector : public IntrospectionEndpoint {
 public:
  static const std::string kBaseRoute;

  CollectorConfigInspector(const std::shared_ptr<CollectorConfig> config);

  // implementation of CivetHandler
  bool handleGet(CivetServer* server, struct mg_connection* conn) override;

 private:
  const std::shared_ptr<CollectorConfig> config_;
  Json::FastWriter writer_;
  Json::Value configToJson();
};

}  // namespace collector

#endif
