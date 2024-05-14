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

bool NetworkStatusInspector::handleGetEndpoints(struct mg_connection* conn, const QueryParams& query_params) {
  collector::AdvertisedEndpointMap endpoint_states = conntracker_->FetchEndpointState(true, false);
  Json::Value body_root(Json::objectValue);

  std::unordered_map<std::string, std::forward_list<collector::ContainerEndpointMap::value_type*>> by_container;

  std::optional<std::string> container_filter = GetParameter(query_params, kQueryParam_container);

  for (auto& endpoint_state : endpoint_states) {
    if (!container_filter || *container_filter == endpoint_state.first.container()) {
      by_container[endpoint_state.first.container()].push_front(&endpoint_state);
    }
  }

  for (auto& container : by_container) {
    const std::string& container_id = container.first;

    Json::Value container_array(Json::arrayValue);

    for (auto endpoint_state : container.second) {
      const collector::ContainerEndpoint& endpoint = endpoint_state->first;
      const collector::ConnStatus& status = endpoint_state->second;

      Json::Value endpoint_node(Json::objectValue);

      endpoint_node["active"] = status.IsActive();

      endpoint_node["l4proto"] = Proto2str(endpoint.l4proto());

      endpoint_node["endpoint"] = Json::objectValue;
      SerializeEndpoint(endpoint.endpoint(), endpoint_node["endpoint"]);

      container_array.append(endpoint_node);
    }

    body_root[container_id] = container_array;
  }

  std::string buffer = writer_.write(body_root);

  mg_send_http_ok(conn, "application/json", buffer.size());

  mg_write(conn, buffer.c_str(), buffer.size());

  return true;
}

bool NetworkStatusInspector::handleGetConnections(struct mg_connection* conn, const QueryParams& query_params) {
  collector::ConnMap connection_states = conntracker_->FetchConnState(true, false);
  Json::Value body_root(Json::objectValue);

  std::unordered_map<std::string, std::forward_list<collector::ConnMap::value_type*>> by_container;

  std::optional<std::string> container_filter = GetParameter(query_params, kQueryParam_container);

  for (auto& connection_state : connection_states) {
    if (!container_filter || *container_filter == connection_state.first.container()) {
      by_container[connection_state.first.container()].push_front(&connection_state);
    }
  }

  for (auto& container : by_container) {
    const std::string& container_id = container.first;

    Json::Value container_array(Json::arrayValue);

    for (auto connection_state : container.second) {
      const collector::Connection& connection = connection_state->first;
      const collector::ConnStatus& status = connection_state->second;

      Json::Value connection_node(Json::objectValue);
      std::string l4proto;

      connection_node["active"] = status.IsActive();

      connection_node["l4proto"] = Proto2str(connection.l4proto());

      if (connection.is_server()) {
        connection_node["from"] = IPNet2str(connection.remote().network());
        connection_node["port"] = connection.local().port();
      } else {
        connection_node["to"] = IPNet2str(connection.remote().network());
        connection_node["port"] = connection.remote().port();
      }

      container_array.append(connection_node);
    }
    body_root[container_id] = container_array;
  }

  std::string buffer = writer_.write(body_root);

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
