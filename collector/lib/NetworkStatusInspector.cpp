#include "NetworkStatusInspector.h"

#include <forward_list>
#include <string>
#include <unordered_map>

#include <arpa/inet.h>

#include "ConnTracker.h"

namespace collector {

const std::string NetworkStatusInspector::kBaseRoute = "/state/network";
const std::string NetworkStatusInspector::kEndpointRoute = kBaseRoute + "/endpoint";
const std::string NetworkStatusInspector::kConnectionRoute = kBaseRoute + "/connection";

const std::string NetworkStatusInspector::kQueryParam_container = "container";

NetworkStatusInspector::NetworkStatusInspector(const std::shared_ptr<ConnectionTracker> conntracker) : conntracker_(conntracker) {
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

static std::string IPNet2str(const IPNet& net) {
  return IP2str(net.address()) + (net.bits() == 0 ? "" : "/" + std::to_string(net.bits()));
}

static void SerializeEndpoint(const Endpoint& ep, Json::Value& node) {
  node["address"] = IPNet2str(ep.network());
  node["port"] = ep.port();
}

bool NetworkStatusInspector::handleGetEndpoints(struct mg_connection* conn, const QueryParams& queryParams) {
  collector::AdvertisedEndpointMap endpointStates = conntracker_->FetchEndpointState(true, false);
  Json::Value bodyRoot(Json::objectValue);

  std::unordered_map<std::string, std::forward_list<collector::ContainerEndpointMap::value_type*>> byContainer;

  std::optional<std::string> containerFilter = GetParameter(queryParams, kQueryParam_container);

  for (auto& endpointState : endpointStates) {
    if (!containerFilter || *containerFilter == endpointState.first.container()) {
      byContainer[endpointState.first.container()].push_front(&endpointState);
    }
  }

  for (auto& container : byContainer) {
    const std::string& containerId = container.first;

    Json::Value containerArray(Json::arrayValue);

    for (auto endpointState : container.second) {
      const collector::ContainerEndpoint& endpoint = endpointState->first;
      const collector::ConnStatus& status = endpointState->second;

      Json::Value endpointNode(Json::objectValue);

      endpointNode["active"] = status.IsActive();

      endpointNode["l4proto"] = Proto2str(endpoint.l4proto());

      endpointNode["endpoint"] = Json::objectValue;
      SerializeEndpoint(endpoint.endpoint(), endpointNode["endpoint"]);

      containerArray.append(endpointNode);
    }

    bodyRoot[containerId] = containerArray;
  }

  std::string buffer = Json::writeString(jsonStreamWriterBuilder_, bodyRoot);

  mg_send_http_ok(conn, "application/json", buffer.size());

  mg_write(conn, buffer.c_str(), buffer.size());

  return true;
}

bool NetworkStatusInspector::handleGetConnections(struct mg_connection* conn, const QueryParams& queryParams) {
  collector::ConnMap connectionStates = conntracker_->FetchConnState(true, false);
  Json::Value bodyRoot(Json::objectValue);

  std::unordered_map<std::string, std::forward_list<collector::ConnMap::value_type*>> byContainer;

  std::optional<std::string> containerFilter = GetParameter(queryParams, kQueryParam_container);

  for (auto& connectionState : connectionStates) {
    if (!containerFilter || *containerFilter == connectionState.first.container())
      byContainer[connectionState.first.container()].push_front(&connectionState);
  }

  for (auto& container : byContainer) {
    const std::string& containerId = container.first;

    Json::Value containerArray(Json::arrayValue);

    for (auto connectionState : container.second) {
      const collector::Connection& connection = connectionState->first;
      const collector::ConnStatus& status = connectionState->second;

      Json::Value connectionNode(Json::objectValue);
      std::string l4proto;

      connectionNode["active"] = status.IsActive();

      connectionNode["l4proto"] = Proto2str(connection.l4proto());

      if (connection.is_server()) {
        connectionNode["from"] = IPNet2str(connection.remote().network());
        connectionNode["port"] = connection.local().port();
      } else {
        connectionNode["to"] = IPNet2str(connection.remote().network());
        connectionNode["port"] = connection.remote().port();
      }

      containerArray.append(connectionNode);
    }
    bodyRoot[containerId] = containerArray;
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
  auto parameters = ParseParameters(req_info->query_string);
  if (uri == kEndpointRoute) {
    return handleGetEndpoints(conn, parameters);
  } else if (uri == kConnectionRoute) {
    return handleGetConnections(conn, parameters);
  }
  return ClientError(conn, "unknown route");
}

}  // namespace collector