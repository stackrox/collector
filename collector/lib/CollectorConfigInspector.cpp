#include "CollectorConfigInspector.h"

#include <string>

#include <Logging.h>

#include <google/protobuf/util/json_util.h>

namespace collector {

const std::string CollectorConfigInspector::kBaseRoute = "/state/config";

CollectorConfigInspector::CollectorConfigInspector(const std::shared_ptr<CollectorConfig> config) : config_(config) {
}

Json::Value CollectorConfigInspector::configToJson() {
  Json::Value root;
  const auto& runtime_config = config_->GetRuntimeConfig();

  if (!runtime_config.has_value()) {
    return root;
  }
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
      CLOG(ERROR) << "Failed to parse JSON string: " << errs;
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

  Json::Value root = configToJson();

  mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n");
  mg_printf(conn, "%s\r\n", writer_.write(root).c_str());

  return true;
}

}  // namespace collector
