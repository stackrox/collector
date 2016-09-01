#ifndef _ROUTER_H_
#define _ROUTER_H_

#include <map>
#include <string>

#include <boost/regex.hpp>

#include "Handler.h"

namespace collector {

class Router {
    public:
    Router();
    ~Router();

    void addGetHandler(const std::string& path, Handler *handler);
    void addPostHandler(const std::string& path, Handler *handler);

    Handler * lookupGetHandler(const std::string &url, std::vector<std::string> &params) const;
    Handler * lookupPostHandler(const std::string &url) const;

    private:
    std::map<boost::regex, Handler *> getHandlers;
    std::map<std::string, Handler *> postHandlers;
};

}   /* namespace collector */

#endif  /* _ROUTER_H_ */

