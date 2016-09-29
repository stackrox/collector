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

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <list>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <json/json.h>
#include <boost/regex.hpp>

extern "C" {
    #include <unistd.h>
    #include <pthread.h>
    #include <curl/curl.h>
}

#include "ContainerMapWatcher.h"
#include "KafkaClient.h"

namespace collector {

static void *watcher_loop(void *arg)
{
    ContainerMapWatcher *watcher = (ContainerMapWatcher *)arg;
    watcher->run();
    return NULL;
}

static size_t
write_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    ContainerMapWatcher *handle = (ContainerMapWatcher *)userdata;
    return handle->handle_write(ptr, size, nmemb);
}

ContainerMapWatcher::ContainerMapWatcher(Sysdig *sysdigService, bool &terminate_flag, unsigned int refreshMs,
    const std::string &serverApiEndpoint, const std::string &nodeName, const std::string &_defaultTopic)
    : sysdig(sysdigService),
      terminate(terminate_flag),
      refreshIntervalMs(refreshMs),
      serverEndpoint(serverApiEndpoint),
      node(nodeName),
      defaultTopic(_defaultTopic),
      tid(0)
{
    curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Unexpected error creating new CURL handle for Container Mapping API");
    }

    CURLcode code;

    std::string mapURL = url();
    code = curl_easy_setopt(curl, CURLOPT_URL, mapURL.c_str());
    std::cout << "Container Map Watcher URL: " << mapURL << std::endl;
    if (code != CURLE_OK) {
        std::string msg("Unexpected error setting URL for Container Mapping API handle: ");
        msg += curl_easy_strerror(code);
        throw std::runtime_error(msg);
    }

    code = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &write_callback);
    if (code != CURLE_OK) {
        std::string msg("Unexpected error setting write function callback for Container Mapping API handle: ");
        msg += curl_easy_strerror(code);
        throw std::runtime_error(msg);
    }

    code = curl_easy_setopt(curl, CURLOPT_WRITEDATA, this);
    if (code != CURLE_OK) {
        std::string msg("Unexpected error setting write data for Container Mapping API handle: ");
        msg += curl_easy_strerror(code);
        throw std::runtime_error(msg);
    }

    code = curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curlErrorBuf);
    if (code != CURLE_OK) {
        std::string msg("Unexpected error setting write data for Container Mapping API handle: ");
        msg += curl_easy_strerror(code);
        throw std::runtime_error(msg);
    }

    // add Authorization header to all CURL requests from this watcher
    // see https://curl.haxx.se/libcurl/c/CURLOPT_HTTPHEADER.html
    char* token = std::getenv("ROX_API_TOKEN");
    if (token == NULL) {
        std::string msg("Unable to find ROX_API_TOKEN in environment variables. Will be unable to authenticate to RoxD.");
        throw std::runtime_error(msg);
    }
    struct curl_slist *authorizationHeader = NULL;
    std::string authorization = "Authorization: " + std::string(token);
    authorizationHeader = curl_slist_append(authorizationHeader, authorization.c_str());
    if (authorizationHeader == NULL) {
        std::string msg("Unexpected error setting Authorization header for Container Mapping API handle. Unable to allocate headers list.");
        throw std::runtime_error(msg);
    }
    code = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, authorizationHeader);
    if (code != CURLE_OK) {
        std::string msg("Unexpected error setting Authorization header for Container Mapping API handle: ");
        msg += curl_easy_strerror(code);
        throw std::runtime_error(msg);
    }

    this->buffer.reserve(CURL_MAX_WRITE_SIZE);

    std::cout << "Initialized Container Map Watcher" << std::endl;
}

ContainerMapWatcher::~ContainerMapWatcher()
{
    curl_easy_cleanup(curl);
}

std::string
ContainerMapWatcher::url() const
{
    std::stringstream ss;
    char *nodeEncoded = curl_easy_escape(curl, node.c_str(), node.size());
    ss <<  "http://" << serverEndpoint << "?node=" << nodeEncoded;
    curl_free(nodeEncoded);
    return std::string(ss.str());
}

size_t
ContainerMapWatcher::handle_write(char *ptr, size_t size, size_t nmemb)
{
    size_t realsize = size * nmemb;
    if (realsize > 0) {
        // stash the data
        buffer.append(ptr, realsize);
    }
    return realsize;
}

void
ContainerMapWatcher::updateContainerMap(const std::string &newMap) {
    Json::Value response;
    Json::Reader reader;
    if (!reader.parse(newMap, response, false)) {
        std::cerr << "Ignoring unparseable JSON from Container Mapping API: " << newMap << std::endl;
        return;
    }
    if (!response.isObject()) {
        std::cerr << "Ignoring JSON from Container Mapping API; not an object: " << newMap << std::endl;
        return;
    }

    sysdig->containers().clear();
    Json::Value::Members members = response.getMemberNames();
    for (Json::Value::Members::iterator it = members.begin(); it != members.end(); ++it) {
        std::string containerID = *it;
        // Invalid container IDs are ignored, and thereby left to the default topic.
        if (!validContainerID(containerID)) {
            continue;
        }
        std::string topic = topicName(response, containerID);
        // Only insert the topic into the map if it is _not_ the default value.
        if (topic != defaultTopic) {
            sysdig->containers()[containerID] = topic;
        }
    }
    sysdig->commit();
}

const bool
ContainerMapWatcher::validContainerID(const std::string &containerID) {
    boost::regex cidRegex("[0-9a-f]{12}");
    boost::smatch result;
    bool matched = boost::regex_match(containerID, result, cidRegex);
    return matched;
}

const std::string
ContainerMapWatcher::topicName(const Json::Value &mappings, const std::string &containerID)
{
    using namespace std;

    Json::Value topic = mappings[containerID];

    if (containerID.length() == 0) {
        cout << "Zero length for container ID. Ignoring." << endl;
        return defaultTopic;
    }

    if (!topic.isString()) {
        cout << "Invalid topic name for container ID " << containerID << " with non-string value" << endl;
        return defaultTopic;
    }

    std::string topicNameString = topic.asString();
    if (topicNameString.length() == 0) {
        cout << "Zero length topic name for container ID " << containerID << endl;
        return defaultTopic;
    }

    return topic.asString();
}

void
ContainerMapWatcher::start()
{
    int err = pthread_create(&tid, NULL, &watcher_loop, (void *)this);
    if (err != 0) {
        perror("pthread_create");
        throw std::runtime_error("Unexpected error creating Container Map Watcher loop thread");
    }
}

void
ContainerMapWatcher::stop()
{
    terminate = true;
    if (tid) {
        int err = pthread_join(tid, NULL);
        if (err != 0) {
            perror("pthread_join");
            throw std::runtime_error("Unexpected error stopping Container Map Watcher loop thread");
        }
    }
    std::cout << "Container Map Watcher stopped" << std::endl;
}

void
ContainerMapWatcher::run()
{
    using namespace std;

    cout << "Starting Container Map Watcher thread" << endl;

    CURLcode code;
    do {
        buffer.clear();
        code = curl_easy_perform(curl);
        if (code != CURLE_OK) {
            std::cerr << "Failed to get container to topic mappings: " << curlErrorBuf << std::endl;
        } else {
            updateContainerMap(buffer);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(refreshIntervalMs));
    } while (!terminate);

    cout << "Stopping Container Map Watcher thread" << endl;
}

}   /* namespace collector */

