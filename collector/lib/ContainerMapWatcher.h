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

#ifndef _CONTAINER_MAP_WATCHER_H_
#define _CONTAINER_MAP_WATCHER_H_

#include <json/json.h>

extern "C" {
    #include <curl/curl.h>
    #include <pthread.h>
}

#include "Sysdig.h"

namespace collector {

class ContainerMapWatcher {
    public:
    ContainerMapWatcher(Sysdig *sysdigService, bool &terminate_flag, unsigned int refreshMs,
        const std::string &serverApiEndpoint, const std::string &nodeName, const std::string &defaultTopic);
    virtual ~ContainerMapWatcher();

    void start();
    void stop();

    void run();

    size_t handle_write(char *ptr, size_t size, size_t nmemb);
    void updateContainerMap(const std::string &newMap);

    std::string url() const;

    private:
    const bool validContainerID(const std::string &containerID);
    const std::string topicName(const Json::Value &mappings, const std::string &containerID);


    Sysdig *sysdig;
    bool &terminate;
    unsigned int refreshIntervalMs;
    std::string serverEndpoint;
    std::string node;
    std::string buffer;
    std::string defaultTopic;

    CURL *curl;
    char curlErrorBuf[CURL_ERROR_SIZE];

    pthread_t tid;
};

}   /* namespace collector */

#endif  /* _CONTAINER_MAP_WATCHER_H_ */
