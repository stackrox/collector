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

