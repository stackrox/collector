/** collector, Copyright (c) 2016 StackRox, Inc.

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

#include "GetStatus.h"
#include "MockSysdig.h"

class GetStatusTest : public ::testing::Test {
    protected:
    GetStatusTest() : handler(NULL) {
        using namespace collector;

        containers["93b38389f7fc"] = "topic-abcdabcdabcd";
        containers["f499d5a814f0"] = "topic-db1d6622bd71";

        stats.nEvents = 1;
        stats.nDrops = 2;
        stats.nPreemptions = 3;
        stats.mLinePeriodicity = 5;
        stats.heartbeatDuration = 8;
        stats.nEventsDelta = 13;
        stats.nDropsDelta = 21;
        stats.nPreemptionsDelta = 34;
        stats.nUpdates = 55;
        stats.nFilteredEvents = 89;
        stats.nodeName = "blargle";

        handler = new GetStatus(&sysdig);
    }

    ~GetStatusTest() {
        if (handler != NULL) {
            delete handler;
            handler = NULL;
        }
    }

    void setupSysdig(bool ready, bool hasSysdigStats) {
        using ::testing::_;
        using ::testing::DoAll;
        using ::testing::Return;
        using ::testing::ReturnRef;
        using ::testing::SetArgReferee;

        EXPECT_CALL(sysdig, ready()).WillRepeatedly(Return(ready));

        if (ready) {
            EXPECT_CALL(sysdig, containers()).WillRepeatedly(ReturnRef(containers));
            EXPECT_CALL(sysdig, stats(_)).WillRepeatedly(DoAll(SetArgReferee<0>(stats), Return(hasSysdigStats)));
        }
    }

    std::map<std::string, std::string> containers;
    collector::MockSysdig sysdig;
    collector::SysdigStats stats;
    collector::GetStatus *handler;
};

TEST_F(GetStatusTest, IsOK) {
    using namespace collector;

    setupSysdig(true, true);

    Response response;
    handler->handleRequest(NULL, response);

    EXPECT_EQ(200, response.statusCode);

    EXPECT_EQ("ok", response.body["status"].asString());

    EXPECT_TRUE(response.body["containers"].isObject());
    EXPECT_EQ(2, response.body["containers"].size());
    EXPECT_EQ("topic-abcdabcdabcd", response.body["containers"]["93b38389f7fc"].asString());
    EXPECT_EQ("topic-db1d6622bd71", response.body["containers"]["f499d5a814f0"].asString());

    EXPECT_TRUE(response.body["sysdig"].isObject());
    EXPECT_EQ("ok", response.body["sysdig"]["status"].asString());
    EXPECT_EQ(1, response.body["sysdig"]["events"].asUInt64());
    EXPECT_EQ(2, response.body["sysdig"]["drops"].asUInt64());
    EXPECT_EQ(3, response.body["sysdig"]["preemptions"].asUInt64());
    EXPECT_EQ(5, response.body["sysdig"]["m_line_periodocity"].asUInt());
    EXPECT_EQ(8, response.body["sysdig"]["heartbeat_duration"].asUInt());
    EXPECT_EQ(13, response.body["sysdig"]["events_delta"].asUInt64());
    EXPECT_EQ(21, response.body["sysdig"]["drops_delta"].asUInt64());
    EXPECT_EQ(34, response.body["sysdig"]["preemptions_delta"].asUInt64());
    EXPECT_EQ(55, response.body["sysdig"]["updates"].asUInt());
    EXPECT_EQ(89, response.body["sysdig"]["filtered_events"].asUInt64());
    EXPECT_EQ("blargle", response.body["sysdig"]["node"].asString());
}

TEST_F(GetStatusTest, IsNotReady) {
    //using namespace collector;

    //setupSysdig(false, true);

    //Response response;
    //handler->handleRequest(NULL, response);

    //EXPECT_EQ(200, response.statusCode);

    //EXPECT_EQ("not ready", response.body["status"].asString());
    //EXPECT_TRUE(response.body["containers"].isObject());
    //EXPECT_TRUE(response.body["containers"].empty());
    //EXPECT_TRUE(response.body["sysdig"].isObject());
    //EXPECT_TRUE(response.body["sysdig"].empty());
}

TEST_F(GetStatusTest, SysdigStatsUnavailable) {
    using namespace collector;

    setupSysdig(true, false);

    Response response;
    handler->handleRequest(NULL, response);

    EXPECT_EQ(200, response.statusCode);

    EXPECT_EQ("ok", response.body["status"].asString());

    EXPECT_TRUE(response.body["containers"].isObject());
    EXPECT_EQ(2, response.body["containers"].size());
    EXPECT_EQ("topic-abcdabcdabcd", response.body["containers"]["93b38389f7fc"].asString());
    EXPECT_EQ("topic-db1d6622bd71", response.body["containers"]["f499d5a814f0"].asString());

    EXPECT_TRUE(response.body["sysdig"].isObject());
    EXPECT_EQ(1, response.body["sysdig"].size());
    EXPECT_EQ("stats unavailable", response.body["sysdig"]["status"].asString());
}
