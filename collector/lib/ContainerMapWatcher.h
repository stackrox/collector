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
