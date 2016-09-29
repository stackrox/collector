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

#include "MockSysdig.h"
#include "ContainerMapWatcher.h"

class ContainerMapWatcherTest : public testing::Test {
    protected:
    ContainerMapWatcherTest() : watcher(NULL) {
        using namespace collector;

        using ::testing::ReturnRef;
        using ::testing::Return;
        EXPECT_CALL(sysdig, containers()).WillRepeatedly(ReturnRef(containers));
        EXPECT_CALL(sysdig, ready()).WillRepeatedly(Return(true));
        EXPECT_CALL(sysdig, commit()).WillRepeatedly(Return(true));

        // We need to provide our mocked Sysdig service to the watcher; all other args are dummy values that may be included in tests.
        bool watcherTerminate = false;
        EXPECT_EQ(setenv("ROX_API_TOKEN", "arbitrary", 0), 0);
        watcher = new ContainerMapWatcher((Sysdig *)&sysdig, watcherTerminate, 1000, "test-endpoint/v0.1/api/containers/topic_map", "test-node", "topic-default");
    }

    ~ContainerMapWatcherTest() {
        if (watcher != NULL) {
            delete watcher;
            watcher = NULL;
        }
    }

    std::map<std::string, std::string> containers;
    collector::MockSysdig sysdig;
    collector::ContainerMapWatcher *watcher;
};

TEST_F(ContainerMapWatcherTest, CorrectURL) {
    std::string url = watcher->url();
    EXPECT_EQ("http://test-endpoint/v0.1/api/containers/topic_map?node=test-node", url);
}

TEST_F(ContainerMapWatcherTest, EmptyMapCausesEmptySysdigContainers) {
    sysdig.containers()["354a65f857de"] = "topic-abcdabcdabcd";

    std::string newMap = "{}";

    watcher->updateContainerMap(newMap);

    EXPECT_EQ(0, sysdig.containers().size());
}

TEST_F(ContainerMapWatcherTest, SomeContainers) {
    using namespace std;

    sysdig.containers()["354a65f857de"] = "topic-abcdabcdabcd";

    string newMap = "{ \"93b38389f7fc\":\"topic-abcdabcdabcd\", \"f499d5a814f0\":\"topic-db1d6622bd71\" }";

    watcher->updateContainerMap(newMap);

    EXPECT_EQ(2, sysdig.containers().size());
    EXPECT_EQ("topic-abcdabcdabcd", sysdig.containers()["93b38389f7fc"]);
    EXPECT_EQ("topic-db1d6622bd71", sysdig.containers()["f499d5a814f0"]);

    map<string, string>::iterator it = sysdig.containers().find("354a65f857de");
    EXPECT_EQ(sysdig.containers().end(), it);
}

TEST_F(ContainerMapWatcherTest, NonJSONMapRejectedWithoutChangingMap) {
    sysdig.containers()["123456789012"] = "topic-abcdabcdabcd";
    sysdig.containers()["abcdef123456"] = "topic-default";

    std::string newMap = "This is totally not JSON! {}";

    watcher->updateContainerMap(newMap);

    EXPECT_EQ(2, sysdig.containers().size());
    EXPECT_EQ("topic-abcdabcdabcd", sysdig.containers()["123456789012"]);
    EXPECT_EQ("topic-default", sysdig.containers()["abcdef123456"]);
}

TEST_F(ContainerMapWatcherTest, MessageNotAnObjectRejectedWithoutChangingMap) {
    sysdig.containers()["123456789012"] = "topic-abcdabcdabcd";
    sysdig.containers()["abcdef123456"] = "topic-default";

    std::string newMap = "[]";

    watcher->updateContainerMap(newMap);

    EXPECT_EQ(2, sysdig.containers().size());
    EXPECT_EQ("topic-abcdabcdabcd", sysdig.containers()["123456789012"]);
    EXPECT_EQ("topic-default", sysdig.containers()["abcdef123456"]);
}

TEST_F(ContainerMapWatcherTest, ContainerValueNotAnObjectRejected) {
    sysdig.containers()["123456789012"] = "topic-abcdabcdabcd";

    std::string newMap = "{ \"abcd12345612\": true }";

    watcher->updateContainerMap(newMap);

    // The invalid ML stack should be interpreted as the default topic, which isn't placed in the map.
    EXPECT_EQ(0, sysdig.containers().size());
}

TEST_F(ContainerMapWatcherTest, IgnoreTooShortContainersIDs) {
    std::string newMap = "{ \"93b38389f7fc\":\"topic-abcdabcdabcd\", \"f499d5\":\"topic-db1d6622bd71\" }";

    watcher->updateContainerMap(newMap);

    EXPECT_EQ(1, sysdig.containers().size());
    EXPECT_EQ("topic-abcdabcdabcd", sysdig.containers()["93b38389f7fc"]);
}

TEST_F(ContainerMapWatcherTest, IgnoreTooLongContainerIDs) {
    std::string newMap = "{ \"93b38389f7fc\":\"topic-abcdabcdabcd\",\"f499d5a814f0432284f9d342c23fcb0a39d42337db1d6622bd71457e7dce1e76\":\"topic-db1d6622bd71\" }";

    watcher->updateContainerMap(newMap);

    EXPECT_EQ(1, sysdig.containers().size());
    EXPECT_EQ("topic-abcdabcdabcd", sysdig.containers()["93b38389f7fc"]);
}

TEST_F(ContainerMapWatcherTest, IgnoreEmptyTopicValues) {
    std::string newMap = "{ \"93b38389f7fc\":\"topic-abcdabcdabcd\",\"f499d5a814f0\":\"\" } }";

    watcher->updateContainerMap(newMap);

    EXPECT_EQ(1, sysdig.containers().size());
    EXPECT_EQ("topic-abcdabcdabcd", sysdig.containers()["93b38389f7fc"]);
}

TEST_F(ContainerMapWatcherTest, IgnoreNumericStackValues) {
    std::string newMap = "{ \"93b38389f7fc\":\"topic-abcdabcdabcd\", \"f499d5a814f0\": 42 }";

    watcher->updateContainerMap(newMap);

    EXPECT_EQ(1, sysdig.containers().size());
    EXPECT_EQ("topic-abcdabcdabcd", sysdig.containers()["93b38389f7fc"]);
}

TEST_F(ContainerMapWatcherTest, IgnoreBooleanTopicValues) {
    std::string newMap = "{ \"93b38389f7fc\":\"topic-abcdabcdabcd\",\"f499d5a814f0\": true }";

    watcher->updateContainerMap(newMap);

    EXPECT_EQ(1, sysdig.containers().size());
    EXPECT_EQ("topic-abcdabcdabcd", sysdig.containers()["93b38389f7fc"]);
}

TEST_F(ContainerMapWatcherTest, IgnoreNullStackValues) {
    std::string newMap = "{ \"93b38389f7fc\":\"topic-abcdabcdabcd\", \"f499d5a814f0\": null } }";

    watcher->updateContainerMap(newMap);

    EXPECT_EQ(1, sysdig.containers().size());
    EXPECT_EQ("topic-abcdabcdabcd", sysdig.containers()["93b38389f7fc"]);
}

TEST_F(ContainerMapWatcherTest, IgnoreEmptyContainerID) {
    std::string newMap = "{ \"\":\"abcdabcdabcd\",\"f499d5a814f0\":\"topic-db1d6622bd71\" } }";

    watcher->updateContainerMap(newMap);

    EXPECT_EQ(1, sysdig.containers().size());
    EXPECT_EQ("topic-db1d6622bd71", sysdig.containers()["f499d5a814f0"]);
}
