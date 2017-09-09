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
#include <map>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string>
#include <sstream>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include "civetweb/CivetServer.h"

#include "GetNetworkHealthStatus.h"

namespace collector {

void closeSock(int sock) {
    int rv = close(sock);
    if (rv != 0) {
        perror("close");
    }
}

bool connectTimeout(const char* host, int port, int timeout)
{
    struct sockaddr_in address;  /* the libc network address data structure */
    short int sock = -1;         /* file descriptor for the network socket */
    fd_set fdset;
    struct timeval tv;

    hostent *record = gethostbyname(host);
    if(record == NULL) {
        std::cerr << "network health check: cannot resolve " << host << std::endl;
        return false;
    }
    in_addr *addr = (in_addr *)record->h_addr;
    std::string ip_address = inet_ntoa(*addr);

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(ip_address.c_str()); /* assign the address */
    address.sin_port = htons(port);            /* translate int2port num */

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("socket");
        std::cerr << "network health check: cannot open socket when connecting to " << host << std::endl;
        return false;
    }

    int rv = fcntl(sock, F_SETFL, O_NONBLOCK);
    if (rv == -1) {
        perror("fcntl");
        std::cerr << "network health check: cannot set nonblocking socket when connecting to " << host << std::endl;
        closeSock(sock);
        return false;
    }

    rv = connect(sock, (struct sockaddr *)&address, sizeof(address));
    if (rv != 0 && errno != EINPROGRESS) {
        perror("connect");
        std::cerr << "network health check: error connecting to " << host << std::endl;
        closeSock(sock);
        return false;
    }

    for (;;) {
        FD_ZERO(&fdset);
        FD_SET(sock, &fdset);
        tv.tv_sec = timeout;
        tv.tv_usec = 0;

        rv = select(sock + 1, NULL, &fdset, NULL, &tv);

        if (errno == EAGAIN) {
            std::cout << "network health check: interrupted... trying again" << std::endl;
            continue;
        }

        if (rv != 1) {
            perror("select");
            std::cerr << "network health check: error selecting socket when connecting to " << host << std::endl;
            closeSock(sock);
            return false;
        }

        int so_error;
        socklen_t len = sizeof so_error;

        rv = getsockopt(sock, SOL_SOCKET, SO_ERROR, &so_error, &len);
        if (rv != 0) {
            perror("getsockopt");
            std::cerr << "network health check: error getting socket error when connecting to " << host << std::endl;
            closeSock(sock);
            return false;
        }

        if (so_error != 0) {
            std::cerr << "network health check: error on socket when connecting to " << host << std::endl;
            closeSock(sock);
            return false;
        }

        closeSock(sock);
        return true;
    }

    return false;
}

static void *health_check_loop(void *arg)
{
    GetNetworkHealthStatus *getNetworkHealthStatus = (GetNetworkHealthStatus *)arg;
    getNetworkHealthStatus->run();
    return NULL;
}

GetNetworkHealthStatus::GetNetworkHealthStatus(bool &terminate_flag)
    : terminate(terminate_flag),
      tid(0),
      lock(PTHREAD_MUTEX_INITIALIZER),
      networkHealthEndpoints(std::map<std::string, NetworkHealthStatus*>())
{
    // read the Kafka broker list from the env, split the commas.
    const char* brokerList = ::getenv("BROKER_LIST");
    std::stringstream ss;
    ss.str(std::string(brokerList));
    std::string token;
    while (std::getline(ss, token, ',')) {
        networkHealthEndpoints[token] = new NetworkHealthStatus("kafka", token);
    }
}

GetNetworkHealthStatus::~GetNetworkHealthStatus()
{
}

void
GetNetworkHealthStatus::run()
{
    using namespace std;

    do {
        for (auto it = networkHealthEndpoints.begin(); it != networkHealthEndpoints.end(); ++it) {

            int rv = pthread_mutex_lock(&lock);
            if (rv != 0) {
                perror("pthread_mutex_lock");
                throw std::runtime_error("Unexpected error locking mutex for endpoint");
            }

            string endpoint = it->first;
            NetworkHealthStatus* value = it->second;

            std::stringstream ss;
            ss.str(endpoint);
            std::string token;
            std::string host;
            int port;
            for (int i = 0; std::getline(ss, token, ':'); i++) {
                if (i == 0) {
                    host = token;
                } else if (i == 1) {
                    port = atoi(token.c_str());
                }
            }

            value->connected = connectTimeout(host.c_str(), port, 1);

            rv = pthread_mutex_unlock(&lock);
            if (rv != 0) {
                perror("pthread_mutex_unlock");
                throw std::runtime_error("Unexpected error unlocking mutex for endpoint");
            }
        }
        sleep(5);
    } while (!terminate);

    std::cerr << "Stopping network health check listening thread" << endl;
}

bool
GetNetworkHealthStatus::handleGet(CivetServer *server, struct mg_connection *conn)
{
    using namespace std;

    Json::Value endpoints(Json::objectValue);

    for (auto it = networkHealthEndpoints.begin(); it != networkHealthEndpoints.end(); ++it) {
        int rv = pthread_mutex_lock(&lock);
        if (rv != 0) {
            perror("pthread_mutex_lock");
            throw std::runtime_error("Unexpected error locking mutex for endpoint");
        }

        string endpoint = it->first;
        NetworkHealthStatus* value = it->second;

        Json::Value endpointJson(Json::objectValue);
        endpointJson["Name"] = value->name;
        endpointJson["Endpoint"] = value->endpoint;
        endpointJson["Connected"] = value->connected;

        endpoints[endpoint] = endpointJson;

        rv = pthread_mutex_unlock(&lock);
        if (rv != 0) {
            perror("pthread_mutex_unlock");
            throw std::runtime_error("Unexpected error unlocking mutex for endpoint");
        }
    }

    Json::StyledStreamWriter writer;
    stringstream out;
    writer.write(out, endpoints);
    const std::string document = out.str();

    mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n");
    mg_printf(conn, "%s\n", document.c_str());
    return true;
}

void
GetNetworkHealthStatus::start()
{
    int rv = 0;
    terminate = false;
    pthread_attr_t attr, *attrptr = nullptr;

    rv = pthread_attr_init(&attr);
    if (rv != 0) {
        perror("pthread_attr_init");
        throw std::runtime_error("Unexpected error creating network health check loop thread");
    }

    rv = pthread_attr_setstacksize(&attr, 1024*1024);
    if (rv != 0) {
        perror("pthread_attr_setstacksize");
        throw std::runtime_error("Unexpected error creating network health check loop thread");
    }

    std::cerr << "Stack size for network health check thread set to 1MB" << std::endl;
    attrptr = &attr;

    rv = pthread_create(&tid, attrptr, &health_check_loop, (void *)this);
    if (rv != 0) {
        perror("pthread_create");
        throw std::runtime_error("Unexpected error creating network health check loop thread");
    }
}

void
GetNetworkHealthStatus::stop()
{
    terminate = true;
    if (tid) {
        int err = pthread_join(tid, NULL);
        if (err != 0) {
            perror("pthread_create");
            throw std::runtime_error("Unexpected error creating network health check loop thread");
        }
    }
    std::cerr << "Network health check stopped" << std::endl;
}

}   /* namespace listener */

