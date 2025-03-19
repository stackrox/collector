#include "GetStatus.h"

#include <string>

#include <json/json.h>

namespace collector {

const std::string GetStatus::kBaseRoute = "/ready";

bool GetStatus::handleGet(CivetServer* server, struct mg_connection* conn) {
  using namespace std;

  Json::Value status(Json::objectValue);

  system_inspector::Stats stats;
  bool ready = system_inspector_->GetStats(&stats);

  if (ready) {
    Json::Value drops = Json::Value(Json::objectValue);
    drops["total"] = Json::UInt64(stats.nDrops);
    drops["ringbuffer"] = Json::UInt64(stats.nDropsBuffer);
    drops["threadcache"] = Json::UInt64(stats.nDropsThreadCache);

    status["status"] = "ok";
    status["collector"] = Json::Value(Json::objectValue);
    status["collector"]["node"] = node_name_;
    status["collector"]["events"] = Json::UInt64(stats.nEvents);
    status["collector"]["drops"] = drops;
    status["collector"]["preemptions"] = Json::UInt64(stats.nPreemptions);

    mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n");
    mg_printf(conn, "%s\n", status.toStyledString().c_str());
  } else {
    mg_printf(conn, "HTTP/1.1 503 Service Unavailable\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n");
    mg_printf(conn, "%s\n", "{}");
  }

  return true;
}

} /* namespace collector */
