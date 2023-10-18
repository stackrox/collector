#ifndef COLLECTOR_NETWORKSTATUSINSPECTOR_H
#define COLLECTOR_NETWORKSTATUSINSPECTOR_H

#include <memory>

#include <json/json.h>

#include "CivetServer.h"
#include "civetweb.h"

namespace collector {

class ConnectionTracker;

class NetworkStatusInspector : public CivetHandler {
 public:
  static const std::string kBaseRoute;
  static const std::string kEndpointRoute;
  static const std::string kConnectionRoute;

  NetworkStatusInspector(const std::shared_ptr<ConnectionTracker> conntracker);

  // implementation of CivetHandler
  bool handleGet(CivetServer* server, struct mg_connection* conn) override;

 private:
  const std::shared_ptr<ConnectionTracker> conntracker_;
  Json::StreamWriterBuilder jsonStreamWriterBuilder_;

  bool handleGetEndpoints(struct mg_connection* conn);
  bool handleGetConnections(struct mg_connection* conn);

  bool ServerError(struct mg_connection* conn, const char* err);
  bool ClientError(struct mg_connection* conn, const char* err);
};
}  // namespace collector

#endif