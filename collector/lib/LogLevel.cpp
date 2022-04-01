#include "LogLevel.h"

#include <string>

#include <json/json.h>

#include "Logging.h"

namespace collector {

bool LogLevel::handlePost(CivetServer* server, struct mg_connection* conn) {
  using namespace std;

  Json::Value response(Json::objectValue);

  char buf[4096];
  int bytes = mg_read(conn, buf, sizeof(buf));
  std::string request(buf, std::max(bytes, 0));

  if (request.empty()) {
    response["status"] = "Ok";
    response["level"] = logging::GetLogLevelName(logging::GetLogLevel());
  } else {
    logging::LogLevel level;
    if (logging::ParseLogLevelName(request, &level)) {
      logging::SetLogLevel(level);
      response["status"] = "Ok";
    } else {
      response["status"] = "Error";
      response["error"] = "Invalid log level '" + request + "'";
    }
  }

  if (response["status"] == "Ok") {
    mg_printf(conn, "HTTP/1.1 200 OK\r\n");
  } else {
    mg_printf(conn, "HTTP/1.1 400 Bad Request\r\n");
  }
  mg_printf(conn, "Content-Type: application/json\r\nConnection: close\r\n\r\n");
  mg_printf(conn, "%s\n", response.toStyledString().c_str());
  return true;
}

}  // namespace collector
