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

#include <iostream>
#include <json/json.h>
#include <string>
#include <sstream>

#include "civetweb/CivetServer.h"

#include "GetNetworkHealthStatus.h"
#include "Logging.h"
#include "Utility.h"

namespace collector {

GetNetworkHealthStatus::GetNetworkHealthStatus(const std::vector<EndpointSpec>& kafka_brokers,
                                               std::shared_ptr<prometheus::Registry> registry)
        : registry_(std::move(registry)) {
    auto& family = prometheus::BuildGauge()
        .Name("rox_network_reachability")
        .Help("Reachability report for every dependency service")
        .Register(*registry_);

    network_statuses_.reserve(kafka_brokers.size());
    for (const auto& broker_addr : kafka_brokers) {
      prometheus::Gauge* gauge = &family.Add({{"endpoint", broker_addr.str()}, {"name", "kafka"}});
      network_statuses_.emplace_back("kafka", broker_addr, gauge);
    }
}

void GetNetworkHealthStatus::run() {
    using namespace std;

    while (thread_.Pause(std::chrono::seconds(5))) {
        for (auto& healthEndpoint : network_statuses_) {
            if (thread_.should_stop()) {
                break;
            }
            std::string error_str;
            ConnectivityStatus conn_status = CheckConnectivity(
                healthEndpoint.address, std::chrono::seconds(1), &error_str,
                [this]{ return thread_.should_stop(); }, thread_.stop_fd());
            if (conn_status == ConnectivityStatus::INTERRUPTED) break;

            healthEndpoint.connected = (conn_status == ConnectivityStatus::OK);
            CLOG_IF(conn_status == ConnectivityStatus::ERROR, ERROR)
                << "Error checking network health status for " << healthEndpoint.address.str() << ": " << error_str;
            healthEndpoint.gauge->Set(healthEndpoint.connected ? 1.0 : 0.0);
        }
    }

    CLOG(INFO) << "Stopping network health check listening thread";
}

bool GetNetworkHealthStatus::handleGet(CivetServer* server, struct mg_connection* conn) {
    Json::Value endpoints(Json::objectValue);

    for (const auto& healthEndpoint : network_statuses_) {
        Json::Value endpointJson(Json::objectValue);
        std::string endpoint = healthEndpoint.endpoint();
        endpointJson["Name"] = healthEndpoint.name;
        endpointJson["Endpoint"] = endpoint;
        endpointJson["Connected"] = healthEndpoint.connected;

        endpoints[endpoint] = endpointJson;
    }

    mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n");
    mg_printf(conn, "%s\n", endpoints.toStyledString().c_str());
    return true;
}

bool GetNetworkHealthStatus::start() {
    if (!thread_.Start(&GetNetworkHealthStatus::run, this)) {
        CLOG(ERROR) << "Failed to start network health status thread";
        return false;
    }
    return true;
}

void GetNetworkHealthStatus::stop() {
    if (!thread_.running()) return;
    thread_.Stop();
    CLOG(INFO) << "Network health check stopped";
}

}  // namespace collector
