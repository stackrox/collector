#ifndef _GET_STATUS_H_
#define _GET_STATUS_H_

#include <map>
#include <string>

#include <json/json.h>

#include "Handler.h"
#include "Request.h"
#include "Response.h"
#include "Sysdig.h"

namespace collector {

class GetStatus : public Handler {
    private:
    Sysdig *sysdig;

    public:
    GetStatus(Sysdig *sysdig);
    virtual ~GetStatus();
    void handleRequest(const Request *request, Response &response);
};

}   /* namespace collector */

#endif  /* _GET_STATUS_H_ */

