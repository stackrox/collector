#pragma once

#include <json/writer.h>

#include "CivetWrapper.h"
#include "CollectorConfig.h"

namespace collector {

class CollectorConfigInspector : public CivetWrapper {
 public:
  CollectorConfigInspector(const CollectorConfig& config);

  // implementation of CivetHandler
  bool handleGet(CivetServer* server, struct mg_connection* conn) override;

  const std::string& GetBaseRoute() override {
    return kBaseRoute;
  }

 private:
  static const std::string kBaseRoute;

  const CollectorConfig& config_;
  std::string configToJson(bool& isError);
};

}  // namespace collector
