#ifndef _GET_CONTAINERS_H_
#define _GET_CONTAINERS_H_

#include <string>

#include <json/json.h>

#include "Handler.h"
#include "Request.h"
#include "Response.h"
#include "Sysdig.h"

namespace collector {

class GetContainers : public Handler {
    private:
    Sysdig *sysdig;

    public:
    GetContainers(Sysdig *sysdig);
    virtual ~GetContainers();
    void handleRequest(const Request *request, Response &response);
};

}   /* namespace collector */

#endif  /* _GET_CONTAINERS_H_ */

