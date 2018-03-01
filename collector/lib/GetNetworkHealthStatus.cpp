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

#include <arpa/inet.h>
#include <fcntl.h>
#include <iostream>
#include <json/json.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <stdio.h>
#include <sstream>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <poll.h>

#include "civetweb/CivetServer.h"

#include "GetNetworkHealthStatus.h"
#include "Logging.h"
#include "Utility.h"

namespace collector {

class AutoCloser {
 public:
  AutoCloser(int* fd) : m_fd(fd) {}
  ~AutoCloser() {
    if (m_fd && *m_fd > 0) {
      int rv = close(*m_fd);
      if (rv != 0) {
        CLOG(WARNING) << "Error closing file descriptor " << *m_fd << ": " << StrError();
      }
    }
  }

 private:
  int* m_fd;
};

bool GetNetworkHealthStatus::checkEndpointStatus(const NetworkHealthStatus& status, std::chrono::milliseconds timeout) {
    const std::string& host = status.host;
    hostent *record = gethostbyname(host.c_str());
    if(record == NULL) {
        CLOG(ERROR) << "network health check: error resolving " << host << ": " << hstrerror(h_errno);
        return false;
    }

    struct sockaddr_in address;
    memcpy(&address.sin_addr.s_addr, record->h_addr, sizeof(in_addr));
    address.sin_family = AF_INET;
    address.sin_port = htons(status.port);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("socket");
        CLOG(ERROR) << "network health check: cannot open socket when connecting to " << host;
        return false;
    }

    AutoCloser closer(&sock);
    int rv = fcntl(sock, F_SETFL, O_NONBLOCK);
    if (rv == -1) {
        perror("fcntl");
        CLOG(ERROR) << "network health check: cannot set nonblocking socket when connecting to " << host;
        return false;
    }

    rv = connect(sock, (struct sockaddr *)&address, sizeof(address));
    if (rv != 0 && errno != EINPROGRESS) {
        CLOG(ERROR) << "network health check: error connecting to " << host << ": " << StrError();
        return false;
    }

    constexpr int num_fds = 2;
    struct pollfd poll_fds[num_fds] = {
        { sock, POLLIN | POLLOUT, 0 },
        { thread_.stop_fd(), POLLIN, 0 },
    };

    auto deadline = std::chrono::steady_clock::now() + timeout;
    while (!thread_.should_stop()) {
        long msec_timeout = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
        if (msec_timeout <= 0) {
            return false;
        }

        for (auto& pollfd : poll_fds) {
            pollfd.revents = 0;
        }
        rv = poll(poll_fds, num_fds, msec_timeout);
        if (rv == 0) {  // timeout
            return false;
        }
        if (rv == -1) {
            if (errno == EAGAIN) {
                CLOG(WARNING) << "network health check: interrupted... trying again";
                continue;
            }

            perror("poll");
            CLOG(ERROR) << "network health check: error polling socket when connecting to " << host;
            return false;
        }

        // Stop signal
        if (poll_fds[1].revents != 0) {
            return false;
        }

        int so_error;
        socklen_t len = sizeof so_error;

        rv = getsockopt(sock, SOL_SOCKET, SO_ERROR, &so_error, &len);
        if (rv != 0) {
            CLOG(ERROR) << "network health check: error getting socket error when connecting to " << host << ": " << StrError();
            return false;
        }

        if (so_error != 0) {
            CLOG(ERROR) << "network health check: error on socket when connecting to " << host << ": " << StrError(so_error);
            return false;
        }

        // Return true if we can either read from or write to the socket, false otherwise.
        return (poll_fds[0].revents & (POLLIN | POLLOUT)) != 0;
    }

    return false;
}

GetNetworkHealthStatus::GetNetworkHealthStatus(const std::string& brokerList, std::shared_ptr<prometheus::Registry> registry)
        : registry_(std::move(registry)) {
    auto& family = prometheus::BuildGauge()
        .Name("rox_network_reachability")
        .Help("Reachability report for every dependency service")
        .Register(*registry_);

    std::stringstream ss;
    ss.str(brokerList);
    std::string token;
    while (std::getline(ss, token, ',')) {
        auto delimPos = token.find(':');
        if (delimPos == std::string::npos) {
            CLOG(ERROR) << "Broker endpoint " << token << " is not of form <host>:<port>";
            continue;
        }
        std::string host = token.substr(0, delimPos);
        std::string portStr = token.substr(delimPos + 1);
        int port = atoi(portStr.c_str());
        if (port <= 0 || port > 65535) {
            CLOG(ERROR) << "Invalid port number: " << portStr;
            continue;
        }
        prometheus::Gauge* gauge = &family.Add({{"endpoint", token}, {"name", "kafka"}});
        network_statuses_.emplace_back("kafka", host, port, gauge);
    }
}

void GetNetworkHealthStatus::run() {
    using namespace std;

    while (thread_.Pause(std::chrono::seconds(5))) {
        for (auto& healthEndpoint : network_statuses_) {
            if (thread_.should_stop()) {
                break;
            }
            healthEndpoint.connected = checkEndpointStatus(healthEndpoint, std::chrono::seconds(1));
            healthEndpoint.gauge->Set(healthEndpoint.connected ? 1.0 : 0.0);
        }
    }

    CLOG(INFO) << "Stopping network health check listening thread";
}

bool GetNetworkHealthStatus::handleGet(CivetServer *server, struct mg_connection *conn) {
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
