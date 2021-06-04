/** collector

A full notice with attributions is provided along with this source code.

This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License version 2 as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program; if not, write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

* In addition, as a special exception, the copyright holders give
* permission to link the code of portions of this program with the
* OpenSSL library under certain conditions as described in each
* individual source file, and distribute linked combinations
* including the two.
* You must obey the GNU General Public License in all respects
* for all of the code used other than OpenSSL.  If you modify
* file(s) with this exception, you may extend this exception to your
* version of the file(s), but you are not obligated to do so.  If you
* do not wish to do so, delete this exception statement from your
* version.
*/

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
