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

#include <map>
#include <string>
#include <sstream>

#include <json/json.h>

#include "civetweb/CivetServer.h"

#include "GetStatus.h"
#include "Sysdig.h"

namespace collector {

GetStatus::GetStatus(Sysdig *sysdigService)
    : sysdig(sysdigService)
{
}

GetStatus::~GetStatus()
{
}

bool
GetStatus::handleGet(CivetServer *server, struct mg_connection *conn)
{
    using namespace std;

    Json::Value status(Json::objectValue);
    status["status"] = sysdig->ready() ? "ok" : "not ready";

    if (sysdig->ready()) {
        SysdigStats stats;
        if (!sysdig->stats(stats)) {
            status["sysdig"] = Json::Value(Json::objectValue);
            status["sysdig"]["status"] = "stats unavailable";
        } else {
            status["sysdig"] = Json::Value(Json::objectValue);
            status["sysdig"]["node"] = stats.nodeName;
            status["sysdig"]["events"] = Json::UInt64(stats.nEvents);
            status["sysdig"]["drops"] = Json::UInt64(stats.nDrops);
            status["sysdig"]["preemptions"] = Json::UInt64(stats.nPreemptions);
            status["sysdig"]["filtered_events"] = Json::UInt64(stats.nFilteredEvents);
        }
    }

    Json::StyledStreamWriter writer;
    stringstream out;
    writer.write(out, status);
    const std::string document = out.str();

    mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n");
    mg_printf(conn, "%s\n", document.c_str());
    return true;
}

}   /* namespace collector */

