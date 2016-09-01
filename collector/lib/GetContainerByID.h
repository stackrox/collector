#ifndef _GET_CONTAINER_BY_ID_H_
#define _GET_CONTAINER_BY_ID_H_

#include <string>
#include <vector>

#include <json/json.h>

#include "Handler.h"
#include "Request.h"
#include "Response.h"
#include "Sysdig.h"

namespace collector {

class GetContainerByID : public Handler {
    private:
    Sysdig *sysdig;

    public:
    GetContainerByID(Sysdig *sysdig);
    virtual ~GetContainerByID();
    void handleRequest(const Request *request, Response &response);
};

}   /* namespace collector */

#endif  /* _GET_CONTAINER_BY_ID_H_ */

