#include "CollectorRuntimeConfigInspector.h"

#include <string>

#include <log/Logging.h>

#include <google/protobuf/util/json_util.h>

namespace collector {

const std::string CollectorConfigInspector::kBaseRoute = "/state/runtime-config";

CollectorConfigInspector::CollectorConfigInspector(const CollectorConfig& config) : config_(config) {
}

std::string CollectorConfigInspector::configToJson(bool& isError) {
  auto lock = config_.ReadLock();
  const auto& runtime_config = config_.GetRuntimeConfig();

  if (!runtime_config.has_value()) {
    return "{}";
  }

  std::string jsonString;
  const auto& config = runtime_config.value();
  google::protobuf::util::JsonPrintOptions options;
  options.always_print_fields_with_no_presence = true;
  absl::Status status = google::protobuf::util::MessageToJsonString(config, &jsonString, options);

  if (!status.ok()) {
    isError = true;
    CLOG(WARNING) << "Failed to convert protobuf object to JSON: " << status.ToString();
    return R"({"error": "Failed to convert protobuf object to JSON"})";
  }

  return jsonString;
}

bool CollectorConfigInspector::handleGet(CivetServer* server, struct mg_connection* conn) {
  const mg_request_info* req_info = mg_get_request_info(conn);

  if (req_info == nullptr) {
    return ServerError(conn, "unable to read request");
  }

  bool isError = false;
  std::string jsonString = configToJson(isError);

  if (isError) {
    mg_printf(conn, "HTTP/1.1 500 Internal Server Error\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n");
  } else {
    mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n");
  }
  mg_printf(conn, "%s\r\n", jsonString.c_str());

  return true;
}

}  // namespace collector
