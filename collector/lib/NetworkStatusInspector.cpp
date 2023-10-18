#include "NetworkStatusInspector.h"

#include <string>

#include <arpa/inet.h>

#include "ConnTracker.h"

namespace collector {

class HTTPChunkedSender : public std::stringbuf {
 public:
  HTTPChunkedSender(struct mg_connection* conn) : std::stringbuf(), conn_(conn) {}

  virtual int sync() override {
    mg_send_chunk(conn_, this->str().c_str(), this->str().length());
    this->str().clear();

    return std::stringbuf::sync();
  }

  virtual int overflow(int c) override {
    if (this->str().length() > 1024)
      sync();

    return std::stringbuf::overflow(c);
  }

 private:
  struct mg_connection* conn_;
};

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

static void SerializeAddress(const Address& addr, Json::Value& node) {
  node["address"] = IP2str(addr);
  node["is_local"] = addr.IsLocal();
  node["is_public"] = addr.IsPublic();
}

static void SerializeEndpoint(const Endpoint& ep, Json::Value& node) {
  node["address"] = Json::objectValue;
  SerializeAddress(ep.address(), node["address"]);
  node["port"] = ep.port();
  node["network"] = IP2str(ep.network().address()) + (ep.network().IsAddress() ? "" : "/" + std::to_string(ep.network().bits()));
}

bool NetworkStatusInspector::handleGetEndpoints(struct mg_connection* conn) {
  collector::ContainerEndpointMap endpointStates = conntracker_->FetchEndpointState(true, false);
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

  mg_send_http_ok(conn, "application/json", -1);

  std::unique_ptr<Json::StreamWriter> writer(jsonStreamWriterBuilder_.newStreamWriter());

  HTTPChunkedSender buffer(conn);
  std::ostream stream(&buffer);

  writer->write(bodyRoot, &stream);
  stream << std::flush;
  mg_send_chunk(conn, NULL, 0);

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

  mg_send_http_ok(conn, "application/json", -1);

  std::unique_ptr<Json::StreamWriter> writer(jsonStreamWriterBuilder_.newStreamWriter());

  HTTPChunkedSender buffer(conn);
  std::ostream stream(&buffer);

  writer->write(bodyRoot, &stream);
  stream << std::flush;
  mg_send_chunk(conn, NULL, 0);

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