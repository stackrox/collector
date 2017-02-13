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

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "CollectorArgs.h"

TEST(CollectorArgs, NoArgs) {
    using namespace collector;
    using ::testing::StartsWith;

    int argc = 0;
    char *argv[0];

    CollectorArgs *args = CollectorArgs::getInstance();
    int exitCode;
    EXPECT_EQ(false, args->parse(argc, argv, exitCode));
    EXPECT_EQ(0, exitCode);
    EXPECT_THAT(args->Message(), StartsWith("USAGE: collector"));
}

TEST(CollectorArgs, ProgramNameOnly) {
    using namespace collector;
    using ::testing::StartsWith;

    int argc = 1;
    const char *argv[] = {
        "collector",
    };

    CollectorArgs *args = CollectorArgs::getInstance();
    int exitCode;
    EXPECT_EQ(false, args->parse(argc, (char **)argv, exitCode));
    EXPECT_EQ(0, exitCode);
    EXPECT_THAT(args->Message(), StartsWith("USAGE: collector"));
}

TEST(CollectorArgs, HelpFlag) {
    using namespace collector;
    using ::testing::StartsWith;

    int argc = 2;
    const char *argv[] = {
        "collector",
        "--help",
    };

    CollectorArgs *args = CollectorArgs::getInstance();
    int exitCode;
    EXPECT_EQ(false, args->parse(argc, (char **)argv, exitCode));
    EXPECT_EQ(0, exitCode);
    EXPECT_THAT(args->Message(), StartsWith("USAGE: collector"));
}

struct CollectorArgsTestCase {
    std::vector<std::string> argv;
    bool                     expectedResult;
    int                      expectedExitCode;
    std::string              expectedMessage;
    std::string              expectedBrokerList;
    unsigned long            expectedMaxContentLengthKB;
    unsigned long            expectedConnectionLimit;
    unsigned long            expectedConnectionLimitPerIP;
    unsigned long            expectedConnectionTimeoutSeconds;

    friend std::ostream& operator<<(std::ostream& os, const CollectorArgsTestCase& obj) {
        std::string argv;
        if (obj.argv.size() > 0) {
            argv = obj.argv[0];
        }
        for (size_t i = 1; i < obj.argv.size(); i++) {
            argv += std::string(" ") + obj.argv[i];
        }

        return os
            << "argv: " << argv
            << " expectedResult: " << obj.expectedResult
            << " expectedExitCode: " << obj.expectedExitCode
            << " expectedMessage: " << obj.expectedMessage
            << " expectedBrokerList " << obj.expectedBrokerList
            << " expectedMaxContentLengthKB: " << obj.expectedMaxContentLengthKB
            << " expectedConnectionLimit: " << obj.expectedConnectionLimit
            << " expectedConnectionLimitPerIP: " << obj.expectedConnectionLimitPerIP
            << " expectedConnectionTimeoutSeconds: " << obj.expectedConnectionTimeoutSeconds;
    }
} testCases[]  = {
    // Unknown flag
    {
        { "collector", "--blargle" },
        false,
        1,
        "Unknown option: --blargle",
        "",
        1024,
        64,
        64,
        8
    }
    // Broker list with one broker
    ,{
        { "collector", "--broker-list=172.16.0.5:9092" },
        true,
        0,
        "",
        "172.16.0.5:9092",
        1024,
        64,
        64,
        8
    }
    // Broker list without an argument
    ,{
        { "collector", "--broker-list" },
        true,
        0,
        "Missing broker list. Cannot configure Kafka client. Reverting to stdout.",
        "",
        1024,
        64,
        64,
        8
    }
    // Malformed broker list
    ,{
        { "collector", "--broker-list=172.16.0.5" },
        false,
        1,
        "Malformed broker",
        "",
        1024,
        64,
        64,
        8
    }
    // Missing broker host
    ,{
        { "collector", "--broker-list=:9092" },
        false,
        1,
        "Missing broker host",
        "",
        1024,
        64,
        64,
        8
    }
    // Missing broker port
    ,{
        { "collector", "--broker-list=172.16.0.5:" },
        false,
        1,
        "Missing broker port",
        "",
        1024,
        64,
        64,
        8
    }
    // Broker list with multiple brokers
    ,{
        { "collector", "--broker-list=172.16.0.5:9092,172.16.0.6:9092" },
        true,
        0,
        "",
        "172.16.0.5:9092,172.16.0.6:9092",
        1024,
        64,
        64,
        8
    }
    // Long broker list
    ,{
        { "collector", std::string("--broker-list=") + std::string(256, 'e') },
        false,
        1,
        "Broker list too long (> 255)",
        "",
        1024,
        64,
        64,
        8
    }
    // Max HTTP Content-Length
    ,{
        { "collector", "--broker-list=172.16.0.5:9092", "--max-content-length=64" },
        true,
        0,
        "",
        "172.16.0.5:9092",
        64,
        64,
        64,
        8
    }
    // Max HTTP Content-Length without an argument
    ,{
        { "collector", "--broker-list=172.16.0.5:9092", "--max-content-length" },
        true,
        0,
        "",
        "172.16.0.5:9092",
        1024,
        64,
        64,
        8
    }
    // Max HTTP Content-Length with non-numeric argument
    ,{
        { "collector", "--broker-list=172.16.0.5:9092", "--max-content-length=blargle" },
        false,
        1,
        "Malformed max HTTP content-length",
        "172.16.0.5:9092",
        1024,
        64,
        64,
        8
    }
    // Max concurrent connections
    ,{
        { "collector", "--broker-list=172.16.0.5:9092", "--connection-limit=32" },
        true,
        0,
        "",
        "172.16.0.5:9092",
        1024,
        32,
        64,
        8
    }
    // Max concurrent connections without an argument
    ,{
        { "collector", "--broker-list=172.16.0.5:9092", "--connection-limit" },
        true,
        0,
        "",
        "172.16.0.5:9092",
        1024,
        64,
        64,
        8
    }
    // Max concurrent connections with a non-numeric argument
    ,{
        { "collector", "--broker-list=172.16.0.5:9092", "--connection-limit=blargle" },
        false,
        1,
        "Malformed connection limit",
        "172.16.0.5:9092",
        1024,
        64,
        64,
        8
    }
    // Max concurrent connections per IP
    ,{
        { "collector", "--broker-list=172.16.0.5:9092", "--per-ip-connection-limit=32" },
        true,
        0,
        "",
        "172.16.0.5:9092",
        1024,
        64,
        32,
        8
    }
    // Max concurrent connections per IP without an argument
    ,{
        { "collector", "--broker-list=172.16.0.5:9092", "--per-ip-connection-limit" },
        true,
        0,
        "",
        "172.16.0.5:9092",
        1024,
        64,
        64,
        8
    }
    // Max concurrent connections per IP with a non-numeric argument
    ,{
        { "collector", "--broker-list=172.16.0.5:9092", "--per-ip-connection-limit=blargle" },
        false,
        1,
        "Malformed per IP connection limit",
        "172.16.0.5:9092",
        1024,
        64,
        64,
        8
    }
    // Connection timeout
    ,{
        { "collector", "--broker-list=172.16.0.5:9092", "--connection-timeout=4" },
        true,
        0,
        "",
        "172.16.0.5:9092",
        1024,
        64,
        64,
        4
    }
    // Connection timeout without an argument
    ,{
        { "collector", "--broker-list=172.16.0.5:9092", "--connection-timeout" },
        true,
        0,
        "",
        "172.16.0.5:9092",
        1024,
        64,
        64,
        8
    }
    // Connection timeout with a non-numeric argument
    ,{
        { "collector", "--broker-list=172.16.0.5:9092", "--connection-timeout=blargle" },
        false,
        1,
        "Malformed connection timeout",
        "172.16.0.5:9092",
        1024,
        64,
        64,
        8
    }
};

class CollectorArgsTest : public ::testing::TestWithParam<CollectorArgsTestCase> {
};

INSTANTIATE_TEST_CASE_P(CollectorArgsTestCases, CollectorArgsTest, ::testing::ValuesIn(testCases));

TEST_P(CollectorArgsTest, Parse) {
    using namespace collector;

    struct CollectorArgsTestCase testCase = GetParam();

    char** argv = new char*[testCase.argv.size()];
    for (size_t i = 0; i < testCase.argv.size(); i++) {
        argv[i] = new char[testCase.argv[i].size() + 1];
        strcpy(argv[i], testCase.argv[i].c_str());
    }

    CollectorArgs *args = CollectorArgs::getInstance();

    int exitCode;
    EXPECT_EQ(testCase.expectedResult, args->parse(testCase.argv.size(), argv, exitCode));
    EXPECT_EQ(testCase.expectedExitCode, exitCode);
    EXPECT_EQ(testCase.expectedMessage, args->Message());
    EXPECT_EQ(testCase.expectedBrokerList, args->BrokerList());
    EXPECT_EQ(testCase.expectedMaxContentLengthKB, args->MaxContentLengthKB());
    EXPECT_EQ(testCase.expectedConnectionLimit, args->ConnectionLimit());
    EXPECT_EQ(testCase.expectedConnectionLimitPerIP, args->ConnectionLimitPerIP());
    EXPECT_EQ(testCase.expectedConnectionTimeoutSeconds, args->ConnectionTimeoutSeconds());

    args->clear();
}

