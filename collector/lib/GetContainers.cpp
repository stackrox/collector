#include <string>

#include <json/json.h>

#include "ContainersMessage.h"
#include "GetContainers.h"
#include "Sysdig.h"

namespace collector {

GetContainers::GetContainers(Sysdig *sysdigService)
    : sysdig(sysdigService)
{
}

GetContainers::~GetContainers()
{
}

void
GetContainers::handleRequest(const Request *request, Response &response)
{
    using namespace std;

    response.statusCode = 200;
    response.body = ContainersMessage(sysdig->containers()).toJSON();
}

}   /* namespace collector */

