#include "ContainerInfoInspector.h"

#include <civetweb.h>
#include <string>
#include <string_view>

namespace collector {
const char* const ContainerInfoInspector::kBaseRoute = "/state/containers/";

bool ContainerInfoInspector::handleGet(CivetServer* server, struct mg_connection* conn) {
  const mg_request_info* req_info = mg_get_request_info(conn);
  if (req_info == nullptr) {
    return ServerError(conn, "unable to read request");
  }

  std::string_view url = req_info->local_uri;
  std::string container_id(url.substr(url.rfind('/') + 1));

  if (container_id.length() != 12) {
    return ClientError(conn, "invalid container ID");
  }

  Json::Value root;

  root["container_id"] = container_id;
  root["namespace"] = std::string(container_metadata_inspector_->GetNamespace(container_id));

  mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n");
  mg_printf(conn, "%s\r\n", writer_.write(root).c_str());

  return true;
}

}  // namespace collector
