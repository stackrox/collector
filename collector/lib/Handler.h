#ifndef _HANDLER_H_
#define _HANDLER_H_

#include "Request.h"
#include "Response.h"

namespace collector {

class Handler {
    public:
    virtual ~Handler() = 0;
    virtual void handleRequest(const Request *request, Response &response) = 0;
};

}   /* namespace collector */

#endif  /* _HANDLER_H_ */

