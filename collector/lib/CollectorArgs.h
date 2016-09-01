#ifndef _COLLECTOR_ARGS_H_
#define _COLLECTOR_ARGS_H_

#define DEFAULT_MAX_HTTP_CONTENT_LENGTH_KB 1024
#define DEFAULT_CONNECTION_LIMIT 64
#define DEFAULT_CONNECTION_LIMIT_PER_IP 64
#define DEFAULT_CONNECTION_TIMEOUT_SECONDS 8
#define DEFAULT_SERVER_ENDPOINT std::string("roxd.stackrox:8888")
#define DEFAULT_MAP_REFRESH_INTERVAL_MS 1000

#include <string>
#include <json/json.h>

#include "optionparser.h"

namespace collector {

class CollectorArgs {
    public:
    static CollectorArgs *getInstance();

    bool parse(int argc, char **argv, int &exitCode);
    void clear();

    option::ArgStatus checkCollectorConfig(const option::Option& option, bool msg);
    option::ArgStatus checkChisel(const option::Option& option, bool msg);
    option::ArgStatus checkBrokerList(const option::Option& option, bool msg);
    option::ArgStatus checkOptionalNumeric(const option::Option& option, bool msg);
    option::ArgStatus checkServerEndpoint(const option::Option& option, bool msg);

    const Json::Value &CollectorConfig() const;
    const std::string &Chisel() const;
    const std::string &BrokerList() const;
    unsigned long MaxContentLengthKB() const;
    unsigned long ConnectionLimit() const;
    unsigned long ConnectionLimitPerIP() const;
    unsigned long ConnectionTimeoutSeconds() const;
    const std::string &ServerEndpoint() const;
    unsigned long MapRefreshInterval() const;
    const std::string &Message() const;

    private:
    CollectorArgs();
    ~CollectorArgs();

    static CollectorArgs *instance;

    Json::Value collectorConfig;
    std::string chisel;
    std::string brokerList;
    unsigned long maxContentLengthKB;
    unsigned long connectionLimit;
    unsigned long connectionLimitPerIP;
    unsigned long connectionTimeoutSeconds;
    std::string serverEndpoint;
    unsigned long mapRefreshInterval;
    std::string message;
};

}   /* namespace collector */

#endif  /* _COLLECTOR_ARGS_H_ */

