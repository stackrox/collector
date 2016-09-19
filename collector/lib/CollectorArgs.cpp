/** collector, Copyright (c) 2016 StackRox, Inc.

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

#include <cstring>
#include <iostream>
#include <sstream>
#include <string>

#include "CollectorArgs.h"

#include "optionparser.h"

namespace collector {

enum optionIndex {
    UNKNOWN,
    HELP,
    COLLECTOR_CONFIG,
    CHISEL,
    BROKER_LIST,
    CONNECTION_LIMIT,
    CONNECTION_LIMIT_PER_IP,
    CONNECTION_TIMEOUT,
    MAX_CONTENT_LENGTH,
    SERVER_ENDPOINT,
    MAP_REFRESH_INTERVAL,
};

static option::ArgStatus
checkCollectorConfig(const option::Option& option, bool msg)
{
    return CollectorArgs::getInstance()->checkCollectorConfig(option, msg);
}

static option::ArgStatus
checkChisel(const option::Option& option, bool msg)
{
    return CollectorArgs::getInstance()->checkChisel(option, msg);
}

static option::ArgStatus
checkBrokerList(const option::Option& option, bool msg)
{
    return CollectorArgs::getInstance()->checkBrokerList(option, msg);
}

static option::ArgStatus
checkOptionalNumeric(const option::Option& option, bool msg)
{
    return CollectorArgs::getInstance()->checkOptionalNumeric(option, msg);
}

static option::ArgStatus
checkServerEndpoint(const option::Option& option, bool msg)
{
    return CollectorArgs::getInstance()->checkServerEndpoint(option, msg);
}

static const option::Descriptor usage[] =
{
    { UNKNOWN,                 0, "", "",                        option::Arg::None,     "USAGE: collector [options]\n\n"
                                                                                        "Options:" },
    { HELP,                    0, "", "help",                    option::Arg::None,     "  --help                \tPrint usage and exit." },
    { COLLECTOR_CONFIG,        0, "", "collector-config",        checkCollectorConfig,  "  --collector-config    \tREQUIRED: Collector config as a JSON string. Please refer to documentation on the valid JSON format." },
    { CHISEL,                  0, "", "chisel",                  checkChisel,           "  --chisel              \tREQUIRED: Chisel is a base64 encoded string." },
    { BROKER_LIST,             0, "", "broker-list",             checkBrokerList,       "  --broker-list         \tREQUIRED: Broker list string in the form HOST1:PORT1,HOST2:PORT2." },
    { MAX_CONTENT_LENGTH,      0, "", "max-content-length",      checkOptionalNumeric,  "  --max-content-length  \tOPTIONAL: Maximum allowed HTTP content-length in KB; default is 1024." },
    { CONNECTION_LIMIT,        0, "", "connection-limit",        checkOptionalNumeric,  "  --connection-limit    \tOPTIONAL: Maximum number of concurrent connections to accept; default is 64." },
    { CONNECTION_LIMIT_PER_IP, 0, "", "per-ip-connection-limit", checkOptionalNumeric,  "  --per-ip-connection-limit \tOPTIONAL: Limit on the number of (concurrent) connections made to the server from the same IP address; default is 64." },
    { CONNECTION_TIMEOUT,      0, "", "connection-timeout",      checkOptionalNumeric,  "  --connection-timeout \tOPTIONAL: After how many seconds of inactivity should a connection be timed out; default is 8 seconds." },
    { SERVER_ENDPOINT,           0, "", "server-endpoint",           checkServerEndpoint,     "  --server-endpoint \tOPTIONAL: The address of the API server endpoint; defaults to roxd.stackrox:8888" },
    { MAP_REFRESH_INTERVAL,    0, "", "map-refresh-interval",    checkOptionalNumeric,  "  --map-refresh-interval \tOPTIONAL: How often (in milliseconds) to refresh container-to-ML-stack mapping; default is 1000."},
    { UNKNOWN,                 0, "", "",                        option::Arg::None,     "\nExamples:\n"
                                                                                        "  collector --broker-list=\"172.16.0.5:9092\"\n"
                                                                                        "  collector --broker-list=\"172.16.0.5:9092\" --max-content-length=1024\n"
                                                                                        "  collector --broker-list=\"172.16.0.5:9092\" --connection-limit=64\n"
                                                                                        "  collector --broker-list=\"172.16.0.5:9092\" --per-ip-connection-limit=64\n"
                                                                                        "  collector --broker-list=\"172.16.0.5:9092\" --connection-timeout=8\n" },
    { 0, 0, 0, 0, 0, 0 },
};

CollectorArgs::CollectorArgs()
    : maxContentLengthKB(DEFAULT_MAX_HTTP_CONTENT_LENGTH_KB),
      connectionLimit(DEFAULT_CONNECTION_LIMIT),
      connectionLimitPerIP(DEFAULT_CONNECTION_LIMIT_PER_IP),
      connectionTimeoutSeconds(DEFAULT_CONNECTION_TIMEOUT_SECONDS),
      serverEndpoint(DEFAULT_SERVER_ENDPOINT),
      mapRefreshInterval(DEFAULT_MAP_REFRESH_INTERVAL_MS)
{
}

CollectorArgs::~CollectorArgs()
{
}

CollectorArgs *CollectorArgs::instance;

CollectorArgs *
CollectorArgs::getInstance() {
    if (instance == NULL) {
        instance = new CollectorArgs();
    }
    return instance;
}

void
CollectorArgs::clear() {
    delete instance;
    instance = new CollectorArgs();
}

bool
CollectorArgs::parse(int argc, char **argv, int &exitCode)
{
    using std::stringstream;

    // Skip program name argv[0] if present
    argc -= (argc > 0);
    argv += (argc > 0);

    option::Stats  stats(usage, argc, argv);
    option::Option options[stats.options_max], buffer[stats.buffer_max];
    option::Parser parse(usage, argc, argv, options, buffer);

    if (parse.error()) {
        exitCode = 1;
        return false;
    }

    if (options[HELP] || argc == 0) {
        stringstream out;
        option::printUsage(out, usage);
        message = out.str();
        exitCode = 0;
        return false;
    }

    if (options[BROKER_LIST]) {
        exitCode = 0;
        return true;
    }

    stringstream out;
    out << "Unknown option: " << options[UNKNOWN].name;
    message = out.str();
    exitCode = 1;
    return false;
}

option::ArgStatus
CollectorArgs::checkChisel(const option::Option& option, bool msg)
{
    using namespace option;
    using std::string;

    if (option.arg == NULL) {
        if (msg) {
            this->message = "Missing chisel. No chisel will be used.";
        }
        return ARG_OK;
    }

    chisel = option.arg;
    std::cout << "Chisel: " << chisel << std::endl;
    return ARG_OK;
}

option::ArgStatus
CollectorArgs::checkCollectorConfig(const option::Option& option, bool msg)
{
    using namespace option;
    using std::string;

    if (option.arg == NULL) {
        if (msg) {
            this->message = "Missing collector config";
        }
        return ARG_ILLEGAL;
    }

    string arg(option.arg);
    std::cout << "Incoming: " << arg << std::endl;

    Json::Value root;
    Json::Reader reader;
    bool parsingSuccessful = reader.parse(arg.c_str(), root);
    if (!parsingSuccessful) {
        if (msg) {
            this->message = "The COLLECTOR_CONFIG is not valid JSON";
        }
        return ARG_ILLEGAL;
    }

    // for now check that the keys exist without checking their types
    if (!root.isMember("syscalls")) {
        if (msg) {
            this->message = "No syscalls key. Will extract on the complete syscall set.";
        }
    }
    if (!root.isMember("format")) {
        if (msg) {
            this->message = "No format. Events will be sent using a default event format.";
        }
    }
    if (!root.isMember("output")) {
        if (msg) {
            this->message = "No output. Events will be sent to stdout.";
        }
    }

    collectorConfig = root;
    std::cout << "Collector config: " << collectorConfig.toStyledString() << std::endl;
    return ARG_OK;
}

option::ArgStatus
CollectorArgs::checkBrokerList(const option::Option& option, bool msg)
{
    using namespace option;
    using std::string;

    if (option.arg == NULL || ::strlen(option.arg) == 0) {
        if (msg) {
            this->message = "Missing broker list. Cannot configure Kafka client. Reverting to stdout.";
        }
        return ARG_OK;
    }

    if (::strlen(option.arg) > 255) {
        if (msg) {
            this->message = "Broker list too long (> 255)";
        }
        return ARG_ILLEGAL;
    }

    string arg(option.arg);
    string::size_type j = arg.find(',');
    if (j != string::npos) {
        if (msg) {
            this->message = "Multiple brokers not supported currently";
        }
        return ARG_ILLEGAL;
    }

    j = arg.find(':');
    if (j == string::npos) {
        if (msg) {
            this->message = "Malformed broker";
        }
        return ARG_ILLEGAL;
    }

    string host = arg.substr(0, j);
    if (host.empty()) {
        if (msg) {
            this->message = "Missing broker host";
        }
        return ARG_ILLEGAL;
    }

    string port = arg.substr(j+1, arg.length());
    if (port.empty()) {
        if (msg) {
            this->message = "Missing broker port";
        }
        return ARG_ILLEGAL;
    }

    brokerList = arg;
    return ARG_OK;
}

option::ArgStatus
CollectorArgs::checkOptionalNumeric(const option::Option& option, bool msg)
{
    using namespace option;
    using std::string;

    if (option.arg == NULL) {
        return ARG_IGNORE;
    }

    // Parse the numeric argument
    char *endptr = NULL;
    unsigned long tmp = strtoul(option.arg, &endptr, 10);
    if (*endptr != '\0') {
        if (msg) {
            switch (option.index()) {
                case MAX_CONTENT_LENGTH:
                    this->message = "Malformed max HTTP content-length";
                    break;
                case CONNECTION_LIMIT:
                    this->message = "Malformed connection limit";
                    break;
                case CONNECTION_LIMIT_PER_IP:
                    this->message = "Malformed per IP connection limit";
                    break;
                case CONNECTION_TIMEOUT:
                    this->message = "Malformed connection timeout";
                    break;
                case MAP_REFRESH_INTERVAL:
                    this->message = "Malformed map refresh interval";
                    break;
            }
        }
        return ARG_ILLEGAL;
    }

    switch (option.index()) {
        case MAX_CONTENT_LENGTH:
            this->maxContentLengthKB = tmp;
            break;
        case CONNECTION_LIMIT:
            this->connectionLimit = tmp;
            break;
        case CONNECTION_LIMIT_PER_IP:
            this->connectionLimitPerIP = tmp;
            break;
        case CONNECTION_TIMEOUT:
            this->connectionTimeoutSeconds = tmp;
            break;
        case MAP_REFRESH_INTERVAL:
            this->mapRefreshInterval = tmp;
            break;
    }

    return ARG_OK;
}

option::ArgStatus
CollectorArgs::checkServerEndpoint(const option::Option& option, bool msg) {
    using namespace option;
    using std::string;

    if (option.arg == NULL) {
        return ARG_IGNORE;
    }

    // Parse the string argument
    if (::strlen(option.arg) > 255) {
        this->message = "API endpoint is too long (limit 255 characters)";
        return ARG_ILLEGAL;
    }

    string arg(option.arg);
    this->serverEndpoint = option.arg;

    return ARG_OK;
}

const Json::Value &
CollectorArgs::CollectorConfig()  const
{
    return collectorConfig;
}

const std::string &
CollectorArgs::Chisel()  const
{
    return chisel;
}

const std::string &
CollectorArgs::BrokerList() const
{
    return brokerList;
}

unsigned long
CollectorArgs::MaxContentLengthKB() const
{
    return maxContentLengthKB;
}

unsigned long
CollectorArgs::ConnectionLimit() const
{
    return connectionLimit;
}

unsigned long
CollectorArgs::ConnectionLimitPerIP() const
{
    return connectionLimitPerIP;
}

unsigned long
CollectorArgs::ConnectionTimeoutSeconds() const
{
    return connectionTimeoutSeconds;
}

const std::string &
CollectorArgs::ServerEndpoint() const
{
    return serverEndpoint;
}

unsigned long
CollectorArgs::MapRefreshInterval() const
{
    return mapRefreshInterval;
}

const std::string &
CollectorArgs::Message() const
{
    return message;
}

}   /* namespace collector */

