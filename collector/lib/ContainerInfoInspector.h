#pragma once

#include <CivetServer.h>
#include <civetweb.h>
#include <string>

#include "CivetWrapper.h"
#include "json/writer.h"

namespace collector {

class ContainerInfoInspector : public CivetWrapper {
 public:
  // implementation of CivetHandler
  bool handleGet(CivetServer* server, struct mg_connection* conn) override;

  const std::string& GetBaseRoute() override {
    return kBaseRoute;
  }

 private:
  static const std::string kBaseRoute;

  Json::FastWriter writer_;
};

}  // namespace collector
