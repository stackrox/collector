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

#include <cstring>
#include <iostream>
#include <sstream>
#include <string>

#include "CollectorArgs.h"

#include "optionparser.h"

#define MAX_CHISEL_LENGTH 8192

namespace collector {

enum optionIndex {
    UNKNOWN,
    HELP,
    COLLECTOR_CONFIG,
    BROKER_LIST,
    CHISEL
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

static const option::Descriptor usage[] =
{
    { UNKNOWN,                 0, "", "",                        option::Arg::None,     "USAGE: collector [options]\n\n"
                                                                                        "Options:" },
    { HELP,                    0, "", "help",                    option::Arg::None,     "  --help                \tPrint usage and exit." },
    { COLLECTOR_CONFIG,        0, "", "collector-config",        checkCollectorConfig,  "  --collector-config    \tREQUIRED: Collector config as a JSON string. Please refer to documentation on the valid JSON format." },
    { CHISEL,                  0, "", "chisel",                  checkChisel,           "  --chisel              \tREQUIRED: Chisel is a base64 encoded string." },
    { BROKER_LIST,             0, "", "broker-list",             checkBrokerList,       "  --broker-list         \tREQUIRED: Broker list string in the form HOST1:PORT1,HOST2:PORT2." },
    { UNKNOWN,                 0, "", "",                        option::Arg::None,     "\nExamples:\n"
                                                                                        "  collector --broker-list=\"172.16.0.5:9092\"\n" },
    { 0, 0, 0, 0, 0, 0 },
};

CollectorArgs::CollectorArgs()
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
    int chiselEncodedLength = chisel.length();
    if (chiselEncodedLength > MAX_CHISEL_LENGTH) {
        if (msg) {
            this->message = "Chisel encoded length cannot exceed " + std::to_string(MAX_CHISEL_LENGTH) + ".";
        }
        return ARG_ILLEGAL;
    }

    std::cout << "Chisel: " << chisel << std::endl;
    return ARG_OK;
}

bool
CollectorArgs::isInvalidFormat(Json::Value root) {
    std::string format = "";
    if (!collectorConfig["format"].isNull()) {
        format = collectorConfig["format"].asString();
    }
    if (format.length() > 0) {
        char* str = new char[format.length() + 1];
        strcpy(str, format.c_str());
        char* token = strtok(str, ",");
        if (token == NULL)
            return true;
        while (token) {
            if (strchr(token, ':') < 0)
                return true;
            token = strtok(NULL, ",");
        }
    }
    return false;
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
            this->message = "A valid JSON configuration is required to start the collector.";
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
    } else if (isInvalidFormat(root)) {
        if (msg) {
            this->message = "Invalid format. The format is expected to be a string of ";
            this->message += "the form label_1:sysdig_field_1, label_2:sysdig_field_2, ...";
        }
        return ARG_ILLEGAL;
    }

    if (!root.isMember("output")) {
        if (msg) {
            this->message = "No output. Events will be sent to stdout.";
        }
    } else {
        std::string output = root["output"].asString();
        if (output != "stdout" && output != "kafka") {
            if (msg) {
                this->message = "The output value has to be either stdout|kafka";
                return ARG_ILLEGAL;
            }
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
    string::size_type j = arg.find(':');
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

const std::string &
CollectorArgs::Message() const
{
    return message;
}

}   /* namespace collector */

