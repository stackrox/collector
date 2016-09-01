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
