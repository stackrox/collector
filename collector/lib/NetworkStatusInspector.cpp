#include "NetworkStatusInspector.h"

#include <string>

#include <arpa/inet.h>

#include "ConnTracker.h"

namespace collector {

const std::string NetworkStatusInspector::kBaseRoute = "/state/network";
const std::string NetworkStatusInspector::kEndpointRoute = kBaseRoute + "/endpoint";
const std::string NetworkStatusInspector::kConnectionRoute = kBaseRoute + "/connection";

NetworkStatusInspector::NetworkStatusInspector(const std::shared_ptr<ConnectionTracker> conntracker) : conntracker_(conntracker) {
}

bool NetworkStatusInspector::ServerError(struct mg_connection* conn, const char* err) {
  return mg_send_http_error(conn, 500, err) >= 0;
}

bool NetworkStatusInspector::ClientError(struct mg_connection* conn, const char* err) {
  return mg_send_http_error(conn, 400, err) >= 0;
}

static std::string IP2str(const Address& addr) {
  switch (addr.family()) {
    case Address::Family::IPV4: {
      char str[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, addr.data(), str, INET_ADDRSTRLEN);
      return str;
    }
    case Address::Family::IPV6: {
      char str[INET6_ADDRSTRLEN];
      inet_ntop(AF_INET6, addr.data(), str, INET6_ADDRSTRLEN);
      return str;
    }
    default:
      break;
  }
  return "unk";
}

static std::string Proto2str(L4Proto proto) {
  switch (proto) {
    case collector::L4Proto::TCP:
      return "TCP";
    case collector::L4Proto::UDP:
      return "UDP";
    case collector::L4Proto::ICMP:
      return "ICMP";
    default:
      break;
  }
  return "UNKNOWN";
}

static void SerializeEndpoint(const Endpoint& ep, Json::Value& node) {
  if (ep.network().IsAddress()) {
    node["address"] = IP2str(ep.address());
  }
  if (ep.network().bits() > 0) {
    node["network"] = IP2str(ep.network().address()) + "/" + std::to_string(ep.network().bits());
  }
  node["port"] = ep.port();
}

bool NetworkStatusInspector::handleGetEndpoints(struct mg_connection* conn) {
  collector::AdvertisedEndpointMap endpointStates = conntracker_->FetchEndpointState(true, false);
  Json::Value bodyRoot(Json::arrayValue);

  for (auto endpointState : endpointStates) {
    Json::Value endpointNode(Json::objectValue);

    endpointNode["active"] = endpointState.second.IsActive();
    endpointNode["container"] = endpointState.first.container();

    endpointNode["l4proto"] = Proto2str(endpointState.first.l4proto());

    endpointNode["endpoint"] = Json::objectValue;
    SerializeEndpoint(endpointState.first.endpoint(), endpointNode["endpoint"]);

    bodyRoot.append(endpointNode);
  }

  std::string buffer = Json::writeString(jsonStreamWriterBuilder_, bodyRoot);

  mg_send_http_ok(conn, "application/json", buffer.size());

  mg_write(conn, buffer.c_str(), buffer.size());

  return true;
}

bool NetworkStatusInspector::handleGetConnections(struct mg_connection* conn) {
  collector::ConnMap connectionStates = conntracker_->FetchConnState(true, false);
  Json::Value bodyRoot(Json::arrayValue);

  for (auto connectionState : connectionStates) {
    Json::Value connectionNode(Json::objectValue);
    std::string l4proto;

    connectionNode["active"] = connectionState.second.IsActive();
    connectionNode["container"] = connectionState.first.container();

    connectionNode["l4proto"] = Proto2str(connectionState.first.l4proto());

    connectionNode["is_server"] = connectionState.first.is_server();

    connectionNode["local"] = Json::objectValue;
    SerializeEndpoint(connectionState.first.local(), connectionNode["local"]);

    connectionNode["remote"] = Json::objectValue;
    SerializeEndpoint(connectionState.first.remote(), connectionNode["remote"]);

    bodyRoot.append(connectionNode);
  }

  std::string buffer = Json::writeString(jsonStreamWriterBuilder_, bodyRoot);

  mg_send_http_ok(conn, "application/json", buffer.size());

  mg_write(conn, buffer.c_str(), buffer.size());

  return true;
}

bool NetworkStatusInspector::handleGet(CivetServer* server, struct mg_connection* conn) {
  const mg_request_info* req_info = mg_get_request_info(conn);
  if (req_info == nullptr) {
    return ServerError(conn, "unable to read request");
  }
  std::string uri = req_info->local_uri;
  if (uri == kEndpointRoute) {
    return handleGetEndpoints(conn);
  } else if (uri == kConnectionRoute) {
    return handleGetConnections((conn));
  }
  return ClientError(conn, "unknown route");
}

}  // namespace collector