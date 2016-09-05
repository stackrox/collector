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

#include <iostream>
#include <string>

#include <json/json.h>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

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
        : server(NULL), curl(NULL), getStatus(NULL)
    {
        using namespace collector;

        using ::testing::_;
        using ::testing::DoAll;
        using ::testing::Return;
        using ::testing::ReturnRef;
        using ::testing::SetArgReferee;
        EXPECT_CALL(sysdig, ready()).WillRepeatedly(Return(true));
        EXPECT_CALL(sysdig, stats(_)).WillRepeatedly(DoAll(SetArgReferee<0>(stats), Return(true)));

        getStatus = new GetStatus(&sysdig);

        router.addGetHandler("/status", getStatus);

        server = new RESTServer(4419, 64, 32, 32, 4, &router);
        server->start();

        curl = curl_easy_init();

        EXPECT_EQ(setenv("API_TOKEN", "arbitrary", 0), 0);
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
    collector::SysdigStats stats;
    collector::Router router;
    collector::MockSysdig sysdig;
    collector::GetStatus *getStatus;
};

TEST_F(APITest, GetStatus) {
   get("http://localhost:4419/status");

    expectResponseCode(200);
    expectContentType("application/json");

    parseJSON();
    EXPECT_EQ("ok", document["status"].asString());
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
