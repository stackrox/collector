#include <map>
#include <string>

#include <json/json.h>

namespace collector {

class ContainersMessage {
    public:
    ContainersMessage(const std::map<std::string, std::string> &containersMap);
    ~ContainersMessage();
    Json::Value toJSON();

    private:
    const std::map<std::string, std::string> &containers;
};

}   /* namespace collector */

