#ifndef COLLECTOR_NETWORKSTATUSINSPECTOR_H
#define COLLECTOR_NETWORKSTATUSINSPECTOR_H

#include <memory>
#include <optional>
#include <unordered_map>

#include <json/writer.h>

#include "IntrospectionEndpoint.h"

namespace collector {

class ConnectionTracker;

class NetworkStatusInspector : public IntrospectionEndpoint {
 public:
  static const std::string kBaseRoute;
  static const std::string kEndpointRoute;
  static const std::string kConnectionRoute;

  NetworkStatusInspector(const std::shared_ptr<ConnectionTracker> conntracker);

  // implementation of CivetHandler
  bool handleGet(CivetServer* server, struct mg_connection* conn) override;

 private:
  const std::shared_ptr<ConnectionTracker> conntracker_;
  Json::StreamWriterBuilder json_stream_writer_builder_;

  static const std::string kQueryParam_container;  // = "container"

  bool handleGetEndpoints(struct mg_connection* conn, const QueryParams& queryParams);
  bool handleGetConnections(struct mg_connection* conn, const QueryParams& queryParams);
};
}  // namespace collector

#endif
