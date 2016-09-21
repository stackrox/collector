/** collector

A full notice with attributions is provided along with this source code.

This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License version 2 as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program; if not, write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

* In addition, as a special exception, the copyright holders give
* permission to link the code of portions of this program with the
* OpenSSL library under certain conditions as described in each
* individual source file, and distribute linked combinations
* including the two.
* You must obey the GNU General Public License in all respects
* for all of the code used other than OpenSSL.  If you modify
* file(s) with this exception, you may extend this exception to your
* version of the file(s), but you are not obligated to do so.  If you
* do not wish to do so, delete this exception statement from your
* version.
*/

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

