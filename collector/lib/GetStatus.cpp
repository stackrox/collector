#include "GetStatus.h"

#include <string>

#include <json/json.h>

namespace collector {

bool GetStatus::handleGet(CivetServer* server, struct mg_connection* conn) {
  using namespace std;

  Json::Value status(Json::objectValue);

  SysdigStats stats;
  bool ready = sysdig_->GetStats(&stats);

  if (ready) {
    status["status"] = "ok";
    status["collector"] = Json::Value(Json::objectValue);
    status["collector"]["node"] = node_name_;
    status["collector"]["events"] = Json::UInt64(stats.nEvents);
    status["collector"]["drops"] = Json::UInt64(stats.nDrops);
    status["collector"]["preemptions"] = Json::UInt64(stats.nPreemptions);
    status["collector"]["filtered_events"] = Json::UInt64(stats.nFilteredEvents);

    mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n");
    mg_printf(conn, "%s\n", status.toStyledString().c_str());
  } else {
    mg_printf(conn, "HTTP/1.1 503 Service Unavailable\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n");
    mg_printf(conn, "%s\n", "{}");
  }

  return true;
}

} /* namespace collector */
