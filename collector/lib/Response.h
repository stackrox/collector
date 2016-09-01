#ifndef _RESPONSE_H_
#define _RESPONSE_H_

#include <json/json.h>

namespace collector {

struct Response {
    unsigned int statusCode;
    Json::Value body;
};

}   /* namespace collector */

#endif  /* _RESPONSE_H_ */

