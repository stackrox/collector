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

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "GetContainers.h"
#include "MockSysdig.h"

class GetContainersTest : public ::testing::Test {
    protected:
    GetContainersTest() : handler(NULL) {
        using namespace collector;

        using ::testing::ReturnRef;
        EXPECT_CALL(sysdig, containers()).WillRepeatedly(ReturnRef(containers));

        handler = new GetContainers(&sysdig);
    }

    ~GetContainersTest() {
        if (handler != NULL) {
            delete handler;
            handler = NULL;
        }
    }

    std::map<std::string, std::string> containers;
    collector::MockSysdig sysdig;
    collector::GetContainers *handler;
};

TEST_F(GetContainersTest, NoContainers) {
    using namespace collector;

    Response response;
    handler->handleRequest(NULL, response);

    EXPECT_EQ(200, response.statusCode);

    EXPECT_TRUE(response.body.isObject());
    EXPECT_TRUE(response.body.empty());
}

TEST_F(GetContainersTest, SomeContainers) {
    using namespace collector;

    sysdig.containers()["93b38389f7fc"] = "topic-abcdabcdabcd";
    sysdig.containers()["f499d5a814f0"] = "topic-db1d6622bd71";

    Response response;
    handler->handleRequest(NULL, response);

    EXPECT_EQ(200, response.statusCode);

    EXPECT_TRUE(response.body.isObject());
    EXPECT_EQ(2, response.body.size());
    EXPECT_EQ("topic-abcdabcdabcd", response.body["93b38389f7fc"].asString());
    EXPECT_EQ("topic-db1d6622bd71", response.body["f499d5a814f0"].asString());
}
