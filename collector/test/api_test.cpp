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

#include <iostream>
#include <map>
#include <string>

#include <json/json.h>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "ContainerMapWatcher.h"
#include "GetContainers.h"
#include "GetContainerByID.h"
#include "GetStatus.h"
#include "MockSysdig.h"
#include "RESTServer.h"
#include "Router.h"
#include "Sysdig.h"

extern "C" {
    #include <curl/curl.h>
}

static size_t
write_callback(char *contents, size_t size, size_t nmemb, void *userdata)
{
    using namespace std;
    size_t realsize = size * nmemb;
    string *responseBody = (string *) userdata;

    responseBody->append(contents, realsize);
    return realsize;
}

class APITest : public ::testing::Test {
    protected:
    APITest()
        : server(NULL), curl(NULL), getStatus(NULL), getContainers(NULL), getContainerByID(NULL)
    {
        using namespace collector;

        using ::testing::_;
        using ::testing::DoAll;
        using ::testing::Return;
        using ::testing::ReturnRef;
        using ::testing::SetArgReferee;
        EXPECT_CALL(sysdig, commit()).WillRepeatedly(Return(true));
        EXPECT_CALL(sysdig, containers()).WillRepeatedly(ReturnRef(containers));
        EXPECT_CALL(sysdig, ready()).WillRepeatedly(Return(true));
        EXPECT_CALL(sysdig, stats(_)).WillRepeatedly(DoAll(SetArgReferee<0>(stats), Return(true)));

        getStatus = new GetStatus(&sysdig);
        getContainers = new GetContainers(&sysdig);
        getContainerByID = new GetContainerByID(&sysdig);

        router.addGetHandler("/status", getStatus);
        router.addGetHandler("/containers", getContainers);
        router.addGetHandler("/containers/([a-zA-Z0-9]{12,64})", getContainerByID);

        server = new RESTServer(4419, 64, 32, 32, 4, &router);
        server->start();

        curl = curl_easy_init();

        // We need to provide our mocked Sysdig service to the watcher; all other args are dummy values.
        bool watcherTerminate = false;
        containerMapWatcher = new ContainerMapWatcher((Sysdig *)&sysdig, watcherTerminate, 1000, "not-an-endpoint", "my-node", "topic-default");
    }

    ~APITest() {
        if (curl != NULL) {
            curl_easy_cleanup(curl);
            curl = NULL;
        }

        if (server != NULL) {
            server->stop();
            delete server;
            server = NULL;
        }

        if (getStatus != NULL) {
            delete getStatus;
            getStatus = NULL;
        }

        if (getContainers != NULL) {
            delete getContainers;
            getContainers = NULL;
        }

        if (getContainerByID != NULL) {
            delete getContainerByID;
            getContainerByID = NULL;
        }

        if (containerMapWatcher != NULL) {
            delete containerMapWatcher;
            containerMapWatcher = NULL;
        }
    }

    void get(const char* url) {
        responseBody.clear();

        curl_easy_reset(curl);

        CURLcode result;

        result = curl_easy_setopt(curl, CURLOPT_URL, url);
        ASSERT_EQ(CURLE_OK, result);

        result = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &write_callback);
        ASSERT_EQ(CURLE_OK, result);

        result = curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);
        ASSERT_EQ(CURLE_OK, result);

        result = curl_easy_perform(curl);
        ASSERT_EQ(CURLE_OK, result);
    }

    void post(const char* url) {
        responseBody.clear();

        curl_easy_reset(curl);

        CURLcode result;

        result = curl_easy_setopt(curl, CURLOPT_URL, url);
        ASSERT_EQ(CURLE_OK, result);

        result = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &write_callback);
        ASSERT_EQ(CURLE_OK, result);

        result = curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);
        ASSERT_EQ(CURLE_OK, result);

        result = curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, requestBody.length());
        ASSERT_EQ(CURLE_OK, result);

        result = curl_easy_setopt(curl, CURLOPT_POSTFIELDS, requestBody.c_str());
        ASSERT_EQ(CURLE_OK, result);

        result = curl_easy_perform(curl);
        ASSERT_EQ(CURLE_OK, result);
    }

    void insertTestContainerMap(const std::string &containerMap) {
        containerMapWatcher->updateContainerMap(containerMap);
    }

    void expectResponseCode(long expected) {
        long responseCode;
        CURLcode result = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
        ASSERT_EQ(CURLE_OK, result);
        EXPECT_EQ(expected, responseCode);
    }

    void expectContentType(const char* expected) {
        char *contentType;
        CURLcode result = curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &contentType);
        ASSERT_EQ(CURLE_OK, result);
        EXPECT_STREQ(expected, contentType);
    }

    void parseJSON() {
        Json::Reader reader;
        EXPECT_TRUE(reader.parse(responseBody, document, false));
        EXPECT_TRUE(document.isObject());
    }

    void buildJSON() {
        using std::stringstream;

        Json::StyledStreamWriter writer;
        stringstream out;
        writer.write(out, document);
        requestBody = out.str();
    }

    collector::RESTServer *server;
    CURL *curl;
    std::string responseBody;
    std::string requestBody;
    Json::Value document;
    std::map<std::string, std::string> containers;
    collector::SysdigStats stats;
    collector::Router router;
    collector::MockSysdig sysdig;
    collector::GetStatus *getStatus;
    collector::GetContainers *getContainers;
    collector::GetContainerByID *getContainerByID;
    collector::ContainerMapWatcher *containerMapWatcher;
};

TEST_F(APITest, GetStatusWhenTheMapIsEmpty) {
    get("http://localhost:4419/status");

    expectResponseCode(200);
    expectContentType("application/json");

    parseJSON();
    EXPECT_EQ("ok", document["status"].asString());
    EXPECT_TRUE(document["containers"].isObject());
    EXPECT_TRUE(document["containers"].empty());
}

TEST_F(APITest, GetContainers) {
    sysdig.containers()["f499d5a814f0"] = "topic-354a65f857de";

    document = Json::Value(Json::objectValue);
    document["93b38389f7fc"] = "topic-abcdabcdabcd";

    buildJSON();
    std::string builtJSON = requestBody;
    insertTestContainerMap(builtJSON);

    get("http://localhost:4419/containers");
    expectResponseCode(200);
    expectContentType("application/json");

    parseJSON();
    EXPECT_EQ(1, document.size());
    EXPECT_EQ(document["93b38389f7fc"].asString(), "topic-abcdabcdabcd");
}

TEST_F(APITest, GetStatus) {
    sysdig.containers()["f499d5a814f0"] = "topic-354a65f857de";

    document = Json::Value(Json::objectValue);
    document["93b38389f7fc"] = "topic-abcdabcdabcd";

    buildJSON();
    std::string builtJSON = requestBody;
    insertTestContainerMap(builtJSON);

    get("http://localhost:4419/status");

    expectResponseCode(200);
    expectContentType("application/json");

    parseJSON();
    EXPECT_EQ("ok", document["status"].asString());
    EXPECT_TRUE(document["containers"].isObject());
    EXPECT_EQ(1, document["containers"].size());
    EXPECT_EQ(document["containers"]["93b38389f7fc"].asString(), "topic-abcdabcdabcd");
}

TEST_F(APITest, GetContainersWhenTheMapIsEmpty) {
    get("http://localhost:4419/containers");

    expectResponseCode(200);
    expectContentType("application/json");

    parseJSON();
    EXPECT_TRUE(document.empty());
}

TEST_F(APITest, GetContainersByID) {
    document = Json::Value(Json::objectValue);
    document["93b38389f7fc"] = "topic-abcdabcdabcd";

    buildJSON();
    std::string builtJSON = requestBody;
    insertTestContainerMap(builtJSON);

    get("http://localhost:4419/containers/93b38389f7fc");

    expectResponseCode(200);
    expectContentType("application/json");

    parseJSON();
    EXPECT_EQ(1, document.size());
    EXPECT_EQ(document["93b38389f7fc"].asString(), "topic-abcdabcdabcd");
}

TEST_F(APITest, GetContainersByLongIDAfterPostingSomeContainers) {
    document = Json::Value(Json::objectValue);
    document["93b38389f7fc"] = "topic-abcdabcdabcd";

    buildJSON();
    std::string builtJSON = requestBody;
    insertTestContainerMap(builtJSON);

    get("http://localhost:4419/containers/93b38389f7fc0e59831a602bdb9c2b6094b1eda71fa2b03908dd390b2ed09e45");

    expectResponseCode(200);
    expectContentType("application/json");

    parseJSON();
    EXPECT_EQ(document["93b38389f7fc"].asString(), "topic-abcdabcdabcd");
}

TEST_F(APITest, GetContainersWhenRequestingAnUnknownContainerID) {
    get("http://localhost:4419/containers/7c49b35af109");

    expectResponseCode(404);
    expectContentType("application/json");

    parseJSON();
    EXPECT_EQ(404, document["code"].asInt());
    EXPECT_EQ("Unknown container ID", document["message"].asString());
    EXPECT_EQ("The specified container doesn't exist.", document["description"].asString());
}

TEST_F(APITest, GetContainersWhenRequestingAnInvalidID) {
    get("http://localhost:4419/containers/foo");

    expectResponseCode(404);
    expectContentType("application/json");

    parseJSON();
    EXPECT_EQ(404, document["code"].asInt());
    EXPECT_EQ("Unknown endpoint", document["message"].asString());
    EXPECT_EQ("The specified URL doesn't exist.", document["description"].asString());
}

TEST_F(APITest, GetAnUnknownEndpoint) {
    get("http://localhost:4419/blargle");

    expectResponseCode(404);
    expectContentType("application/json");

    parseJSON();
    EXPECT_EQ(404, document["code"].asInt());
    EXPECT_EQ("Unknown endpoint", document["message"].asString());
    EXPECT_EQ("The specified URL doesn't exist.", document["description"].asString());
}

TEST_F(APITest, PostToAnUnknownEndpoint) {
    post("http://localhost:4419/blargle");

    expectResponseCode(404);
    expectContentType("application/json");

    parseJSON();
    EXPECT_EQ(404, document["code"].asInt());
    EXPECT_EQ("Unknown endpoint", document["message"].asString());
    EXPECT_EQ("The specified URL doesn't exist.", document["description"].asString());
}

// Test for missing Content-Length header
// Test for POST body length doesn't match Content-Length
