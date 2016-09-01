#include <map>
#include <string>

#include <boost/regex.hpp>

#include "Handler.h"
#include "Router.h"

namespace collector {

Router::Router()
{
}

Router::~Router()
{
}

void
Router::addGetHandler(const std::string& path, Handler *handler)
{
    boost::regex expr(path);
    getHandlers[expr] = handler;
}

void
Router::addPostHandler(const std::string& path, Handler *handler)
{
    postHandlers[path] = handler;
}

Handler *
Router::lookupGetHandler(const std::string &url, std::vector<std::string> &params) const
{
    using namespace std;

    Handler *handler = NULL;

    for (map<boost::regex, Handler*>::const_iterator i = getHandlers.begin();
        i != getHandlers.end(); ++i) {
        boost::regex re = i->first;

        boost::smatch result;
        if (boost::regex_match(url, result, re)) {
            handler = i->second;
            params = vector<string>(result.begin() + 1, result.end());
            break;
        }
    }

    return handler;
}

Handler *
Router::lookupPostHandler(const std::string &url) const
{
    using namespace std;

    Handler *handler = NULL;

    map<string, Handler *>::const_iterator it = postHandlers.find(url);
    if (it != postHandlers.end()) {
        handler = it->second;
    }

    return handler;
}

}   /* namespace collector */

