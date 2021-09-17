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

#include "CollectorArgs.h"

#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

TEST(CollectorArgs, NoArgs) {
  using namespace collector;
  using ::testing::StartsWith;

  int argc = 0;
  char* argv[0];

  CollectorArgs* args = CollectorArgs::getInstance();
  int exitCode;
  EXPECT_EQ(false, args->parse(argc, argv, exitCode));
  EXPECT_EQ(0, exitCode);
  EXPECT_THAT(args->Message(), StartsWith("USAGE: collector"));
}

TEST(CollectorArgs, ProgramNameOnly) {
  using namespace collector;
  using ::testing::StartsWith;

  int argc = 1;
  const char* argv[] = {
      "collector",
  };

  CollectorArgs* args = CollectorArgs::getInstance();
  int exitCode;
  EXPECT_EQ(false, args->parse(argc, (char**)argv, exitCode));
  EXPECT_EQ(0, exitCode);
  EXPECT_THAT(args->Message(), StartsWith("USAGE: collector"));
}

TEST(CollectorArgs, HelpFlag) {
  using namespace collector;
  using ::testing::StartsWith;

  int argc = 2;
  const char* argv[] = {
      "collector",
      "--help",
  };

  CollectorArgs* args = CollectorArgs::getInstance();
  int exitCode;
  EXPECT_EQ(false, args->parse(argc, (char**)argv, exitCode));
  EXPECT_EQ(0, exitCode);
  EXPECT_THAT(args->Message(), StartsWith("USAGE: collector"));
}

struct CollectorArgsTestCase {
  std::vector<std::string> argv;
  bool expectedResult;
  int expectedExitCode;
  std::string expectedMessage;
  std::string expectedGRPCServer;

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
           << " expectedMessage: " << obj.expectedMessage;
  }
} testCases[] = {
    // Unknown flag
    {
        {"collector", "--blargle"},
        false,
        1,
        "Unknown option: --blargle",
    }
    // GRPC server
    ,
    {{"collector", "--grpc-server=172.16.0.5:9092"},
     true,
     0,
     "",
     "172.16.0.5:9092"}
    // GRPC server without an argument
    ,
    {{"collector", "--grpc-server"},
     true,
     0,
     "Missing grpc list. Cannot configure GRPC client. Reverting to stdout.",
     ""}
    // Malformed GRPC server
    ,
    {{"collector", "--grpc-server=172.16.0.5"},
     false,
     1,
     "Malformed grpc server addr",
     ""}
    // Missing GRPC server host
    ,
    {{"collector", "--grpc-server=:9092"},
     false,
     1,
     "Missing grpc host",
     ""}
    // Missing GRPC server port
    ,
    {{"collector", "--grpc-server=172.16.0.5:"},
     false,
     1,
     "Missing grpc port",
     ""}
    // Long GRPC server
    ,
    {{"collector", std::string("--grpc-server=") + std::string(256, 'e')},
     false,
     1,
     "GRPC Server addr too long (> 255)",
     ""}};

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

  CollectorArgs* args = CollectorArgs::getInstance();

  int exitCode;
  EXPECT_EQ(testCase.expectedResult, args->parse(testCase.argv.size(), argv, exitCode));
  EXPECT_EQ(testCase.expectedExitCode, exitCode);
  EXPECT_EQ(testCase.expectedMessage, args->Message());
  EXPECT_EQ(testCase.expectedGRPCServer, args->GRPCServer());

  args->clear();
  for (size_t i = 0; i < testCase.argv.size(); i++) {
    delete[] argv[i];
  }
  delete[] argv;
}
