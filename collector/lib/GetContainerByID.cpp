#include <string>
#include <vector>

#include <json/json.h>

#include "GetContainerByID.h"
#include "Request.h"
#include "Response.h"
#include "Sysdig.h"

namespace collector {

GetContainerByID::GetContainerByID(Sysdig *sysdigService)
    : sysdig(sysdigService)
{
}

GetContainerByID::~GetContainerByID()
{
}

void
GetContainerByID::handleRequest(const Request *request, Response &response)
{
    using namespace std;

    string id = string(request->params[0]).substr(0, 12);

    map<string, string>::iterator it = sysdig->containers().find(id);
    if (it == sysdig->containers().end()) {
        Json::Value status(Json::objectValue);
        status["code"] = 404;
        status["message"] = "Unknown container ID";
        status["description"] = "The specified container doesn't exist.";
        response.statusCode = 404;
        response.body = status;
        return;
    }

    string container = it->first;
    string topic = it->second;

    Json::Value status(Json::objectValue);
    status[container] = topic;

    response.statusCode = 200;
    response.body = status;
}

}   /* namespace collector */

