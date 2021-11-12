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

#include "HostInfo.cpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using namespace testing;

namespace collector {

class MockHostInfo : public HostInfo {
 public:
  MockHostInfo() = default;

  // Mocking just the GetOSReleaseValue method, so we can test everything
  // else
  MOCK_METHOD1(GetOSReleaseValue, std::string(const char*));
};

TEST(KernelVersionTest, TestParseWithBuildID) {
  KernelVersion version = KernelVersion("5.1.10-123", "");
  EXPECT_EQ(5, version.kernel);
  EXPECT_EQ(1, version.major);
  EXPECT_EQ(10, version.minor);
  EXPECT_EQ(123, version.build_id);
}

TEST(KernelVersionTest, TestParseWithoutBuildID) {
  KernelVersion version = KernelVersion("5.1.10", "");
  EXPECT_EQ(5, version.kernel);
  EXPECT_EQ(1, version.major);
  EXPECT_EQ(10, version.minor);
  EXPECT_EQ(0, version.build_id);
}

TEST(KernelVersionTest, TestParseWithAdditional) {
  KernelVersion version = KernelVersion("5.10.25-linuxkit", "");
  EXPECT_EQ(5, version.kernel);
  EXPECT_EQ(10, version.major);
  EXPECT_EQ(25, version.minor);
  EXPECT_EQ(0, version.build_id);
}

TEST(KernelVersionTest, TestParseWithBuildIDAndAdditional) {
  KernelVersion version = KernelVersion("3.10.0-957.10.1.el7.x86_64", "");
  EXPECT_EQ(3, version.kernel);
  EXPECT_EQ(10, version.major);
  EXPECT_EQ(0, version.minor);
  EXPECT_EQ(957, version.build_id);
}

TEST(KernelVersionTest, TestParseInvalidRelease) {
  KernelVersion version = KernelVersion("not.a.release", "");
  EXPECT_EQ(0, version.kernel);
  EXPECT_EQ(0, version.major);
  EXPECT_EQ(0, version.minor);
  EXPECT_EQ(0, version.build_id);
}

TEST(KernelVersionTest, TestParseKnownReleaseStrings) {
  struct kernel {
    const char* release_string;
    int major;
    int minor;
    int patch;
    int build_id;
  } test_data[] = {
      // RHEL 7.6
      {.release_string = "3.10.0-957.10.1.el7.x86_64",
       .major = 3,
       .minor = 10,
       .patch = 0,
       .build_id = 957},
      // Docker Desktop
      {.release_string = "5.10.25-linuxkit",
       .major = 5,
       .minor = 10,
       .patch = 25,
       .build_id = 0},
  };

  for (auto k : test_data) {
    KernelVersion version = KernelVersion(k.release_string, "");
    EXPECT_EQ(k.major, version.kernel);
    EXPECT_EQ(k.minor, version.major);
    EXPECT_EQ(k.patch, version.minor);
    EXPECT_EQ(k.build_id, version.build_id);
    EXPECT_EQ(k.release_string, version.release);
  }
}

TEST(KernelVersionTest, TestVersionStringPopulated) {
  // We're not doing anything with the version string asside from storing it
  // for other things to process, so we can simply test that it is populated
  // correctly in the KernelVersion class
  KernelVersion version = KernelVersion("5.10.3", "this-is-a-version-string");
  EXPECT_EQ("this-is-a-version-string", version.version);
}

TEST(KernelVersionTest, TestHasEBPFSupport) {
  KernelVersion new_kernel("5.10.0", "");
  EXPECT_EQ(true, new_kernel.HasEBPFSupport());

  KernelVersion old_kernel("2.6.0", "");
  EXPECT_EQ(false, old_kernel.HasEBPFSupport());

  KernelVersion old_four_kernel("4.10.0", "");
  EXPECT_EQ(false, old_four_kernel.HasEBPFSupport());

  KernelVersion new_four_kernel("4.20.0", "");
  EXPECT_EQ(true, new_four_kernel.HasEBPFSupport());
}

TEST(HostInfoTest, TestIsRHEL76) {
  KernelVersion kernel("3.10.0-957.10.1.el7.x86_64", "");
  std::string os_id = "rhel";
  EXPECT_TRUE(isRHEL76(kernel, os_id));

  os_id = "coreos";
  EXPECT_FALSE(isRHEL76(kernel, os_id));

  kernel = KernelVersion("5.10.0", "");
  EXPECT_FALSE(isRHEL76(kernel, os_id));
}

TEST(HostInfoTest, TestHasEBPFSupport) {
  KernelVersion kernel("3.10.0-957.10.1.el7.x86_64", "");
  std::string os_id = "rhel";
  EXPECT_TRUE(hasEBPFSupport(kernel, os_id));

  os_id = "coreos";
  EXPECT_FALSE(hasEBPFSupport(kernel, os_id));

  kernel = KernelVersion("5.10.0", "");
  EXPECT_TRUE(hasEBPFSupport(kernel, os_id));
}

TEST(HostInfoTest, TestHostInfoGetDistro) {
  MockHostInfo host;
  EXPECT_CALL(host, GetOSReleaseValue(StrEq("PRETTY_NAME")))
      .WillOnce(Return("SomeDistro"));
  EXPECT_EQ("SomeDistro", host.GetDistro());

  // Verify that we don't re-inititialize state
  EXPECT_CALL(host, GetOSReleaseValue(StrEq("PRETTY_NAME")))
      .Times(0);
  EXPECT_EQ("SomeDistro", host.GetDistro());
}

TEST(HostInfoTest, TestHostInfoGetDistroLinux) {
  MockHostInfo host;
  EXPECT_CALL(host, GetOSReleaseValue(StrEq("PRETTY_NAME")))
      .WillOnce(Return(""));
  EXPECT_EQ("Linux", host.GetDistro());
}

TEST(HostInfoTest, TestHostInfoGetBuildID) {
  MockHostInfo host;
  EXPECT_CALL(host, GetOSReleaseValue(StrEq("BUILD_ID")))
      .WillOnce(Return("SomeBuildID"));
  EXPECT_EQ("SomeBuildID", host.GetBuildID());
  // Verify that we don't re-inititialize state
  EXPECT_CALL(host, GetOSReleaseValue(StrEq("BUILD_ID")))
      .Times(0);
  EXPECT_EQ("SomeBuildID", host.GetBuildID());
}

TEST(HostInfoTest, TestHostInfoGetOSID) {
  MockHostInfo host;
  EXPECT_CALL(host, GetOSReleaseValue(StrEq("ID")))
      .WillOnce(Return("SomeOSID"));
  EXPECT_EQ("SomeOSID", host.GetOSID());
  EXPECT_CALL(host, GetOSReleaseValue(StrEq("ID")))
      .Times(0);
  EXPECT_EQ("SomeOSID", host.GetOSID());
}

TEST(HostInfoTest, TestHostInfoIsCOS) {
  MockHostInfo host;
  EXPECT_CALL(host, GetOSReleaseValue(StrEq("ID")))
      .WillOnce(Return("cos"));
  EXPECT_CALL(host, GetOSReleaseValue(StrEq("BUILD_ID")))
      .WillOnce(Return("123"));
  EXPECT_TRUE(host.IsCOS());
}

TEST(HostInfoTest, TestHostInfoIsNotCOS) {
  MockHostInfo hostIDWrong;
  EXPECT_CALL(hostIDWrong, GetOSReleaseValue(StrEq("ID")))
      .WillOnce(Return("Docker Desktop"));
  EXPECT_FALSE(hostIDWrong.IsCOS());

  MockHostInfo hostBuildIDWrong;
  EXPECT_CALL(hostBuildIDWrong, GetOSReleaseValue(StrEq("ID")))
      .WillOnce(Return("cos"));
  EXPECT_CALL(hostBuildIDWrong, GetOSReleaseValue(StrEq("BUILD_ID")))
      .WillOnce(Return(""));
  EXPECT_FALSE(hostBuildIDWrong.IsCOS());
}

TEST(HostInfoTest, TestHostInfoIsCoreOS) {
  MockHostInfo host;
  EXPECT_CALL(host, GetOSReleaseValue(StrEq("ID")))
      .WillOnce(Return("coreos"));
  EXPECT_TRUE(host.IsCoreOS());
}

TEST(HostInfoTest, TestHostInfoIsNotCoreOS) {
  MockHostInfo host;
  EXPECT_CALL(host, GetOSReleaseValue(StrEq("ID")))
      .WillOnce(Return("Docker Desktop"));
  EXPECT_FALSE(host.IsCoreOS());
}

TEST(HostInfoTest, TestHostInfoIsDockerDesktop) {
  MockHostInfo host;
  EXPECT_CALL(host, GetOSReleaseValue(StrEq("PRETTY_NAME")))
      .WillOnce(Return("Docker Desktop"));
  EXPECT_TRUE(host.IsDockerDesktop());
}

TEST(HostInfoTest, TestHostInfoIsNotDockerDesktop) {
  MockHostInfo host;
  EXPECT_CALL(host, GetOSReleaseValue(StrEq("PRETTY_NAME")))
      .WillOnce(Return("coreos"));
  EXPECT_FALSE(host.IsDockerDesktop());
}

TEST(HostInfoTest, TestFilterValueNoQuotes) {
  std::stringstream stream;
  stream << "KEY=Value\n";
  EXPECT_EQ("Value", filterForKey(stream, "KEY"));
}

TEST(HostInfoTest, TestFilterValueQuotes) {
  std::stringstream stream;
  stream << "KEY=\"Value Value\"\n";
  EXPECT_EQ("Value Value", filterForKey(stream, "KEY"));
}

TEST(HostInfoTest, TestFilterValueFound) {
  std::stringstream stream;
  stream << "KEY1=Value1\n"
         << "KEY2=Value2\n"
         << "KEY3=Value3\n";
  EXPECT_EQ("Value2", filterForKey(stream, "KEY2"));
}

TEST(HostInfoTest, TestFilterValueNotFound) {
  std::stringstream stream;
  stream << "KEY1=Value1\n"
         << "KEY2=Value2\n"
         << "KEY3=Value3\n";
  EXPECT_EQ("", filterForKey(stream, "Invalid"));
}

}  // namespace collector
