#include <map>
#include <string>

#include <json/json.h>

#include "ContainersMessage.h"

namespace collector {

ContainersMessage::ContainersMessage(const std::map<std::string, std::string> &containersMap)
    : containers(containersMap)
{
}

ContainersMessage::~ContainersMessage()
{
}

Json::Value
ContainersMessage::toJSON()
{
    using namespace std;

    Json::Value result(Json::objectValue);
    for (map<string, string>::const_iterator it = containers.begin();
        it != containers.end(); ++it) {
        result[it->first] = it->second;
    }
    return result;
}

}   /* namespace collector */

