#ifndef COLLECTOR_CONFIG_INSPECTOR_H
#define COLLECTOR_CONFIG_INSPECTOR_H

#include <json/writer.h>

#include "CollectorConfig.h"
#include "IntrospectionEndpoint.h"

namespace collector {

class CollectorConfigInspector : public IntrospectionEndpoint {
 public:
  static const std::string kBaseRoute;

  CollectorConfigInspector(const CollectorConfig& config);

  // implementation of CivetHandler
  bool handleGet(CivetServer* server, struct mg_connection* conn) override;

 private:
  const CollectorConfig& config_;
  std::string configToJson(bool& isError);
};

}  // namespace collector

#endif
