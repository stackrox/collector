#ifndef COLLECTOR_NETWORKSTATUSINSPECTOR_H
#define COLLECTOR_NETWORKSTATUSINSPECTOR_H

#include <memory>

#include <json/writer.h>

#include "IntrospectionEndpoint.h"

namespace collector {

class ConnectionTracker;

class NetworkStatusInspector : public IntrospectionEndpoint {
 public:
  NetworkStatusInspector(const std::shared_ptr<ConnectionTracker> conntracker);

  // implementation of CivetHandler
  bool handleGet(CivetServer* server, struct mg_connection* conn) override;

  const std::string& GetBaseRoute() override {
    return kBaseRoute;
  }

 private:
  static const std::string kBaseRoute;
  static const std::string kEndpointRoute;
  static const std::string kConnectionRoute;

  const std::shared_ptr<ConnectionTracker> conntracker_;
  Json::FastWriter writer_;

  static const std::string kQueryParam_container;  // = "container"
  static const std::string kQueryParam_normalize;  // = "normalize"

  bool shouldNormalize(const QueryParams& queryParams) const;

  bool handleGetEndpoints(struct mg_connection* conn, const QueryParams& queryParams);
  bool handleGetConnections(struct mg_connection* conn, const QueryParams& queryParams);
};
}  // namespace collector

#endif
