#include "CollectorConfigInspector.h"

#include <forward_list>
#include <string>
#include <unordered_map>

#include <arpa/inet.h>

#include <google/protobuf/util/json_util.h>

#include "ConnTracker.h"

namespace collector {

const std::string CollectorConfigInspector::kBaseRoute = "/state/config";

CollectorConfigInspector::CollectorConfigInspector(const std::shared_ptr<CollectorConfig> config) : config_(config) {
}

Json::Value CollectorConfigInspector::configToJson() {
  Json::Value root;
  std::optional<sensor::CollectorConfig> runtime_config = config_->GetRuntimeConfig();

  if (runtime_config.has_value()) {
    std::string jsonString;
    const auto& config = runtime_config.value();
    google::protobuf::util::Status status = google::protobuf::util::MessageToJsonString(config, &jsonString);

    if (!status.ok()) {
      CLOG(WARNING) << "Failed to convert protobuf object to JSON: " << status.ToString();
      return Json::Value();
    }

    Json::CharReaderBuilder readerBuilder;
    std::string errs;
    std::istringstream iss(jsonString);
    if (!Json::parseFromStream(readerBuilder, iss, &root, &errs)) {
      std::cerr << "Failed to parse JSON string: " << errs;
      return Json::Value();
    }
  }

  return root;
}

bool CollectorConfigInspector::handleGet(CivetServer* server, struct mg_connection* conn) {
  const mg_request_info* req_info = mg_get_request_info(conn);

  if (req_info == nullptr) {
    return ServerError(conn, "unable to read request");
  }

  // std::string_view url = req_info->local_uri;

  Json::Value root = configToJson();

  mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n");
  mg_printf(conn, "%s\r\n", writer_.write(root).c_str());

  return true;
}

}  // namespace collector
