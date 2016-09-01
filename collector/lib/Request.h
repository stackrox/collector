#ifndef _REQUEST_H_
#define _REQUEST_H_

#include <string>
#include <vector>

namespace collector {

struct Request {
    std::string body;
    std::vector<std::string> params;
};

}   /* namespace collector */

#endif  /* _REQUEST_H_ */

