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

#include "HostInfo.h"
#include "gtest/gtest.h"

namespace collector {

TEST(KernelVersionTest, TestParseWithBuildID) {
  KernelVersion version = KernelVersion("5.1.10-123", "");
  EXPECT_EQ(5, version.major);
  EXPECT_EQ(1, version.minor);
  EXPECT_EQ(10, version.patch);
  EXPECT_EQ(123, version.build_id);
}

TEST(KernelVersionTest, TestParseWithoutBuildID) {
  KernelVersion version = KernelVersion("5.1.10", "");
  EXPECT_EQ(5, version.major);
  EXPECT_EQ(1, version.minor);
  EXPECT_EQ(10, version.patch);
  EXPECT_EQ(0, version.build_id);
}

TEST(KernelVersionTest, TestParseWithAdditional) {
  KernelVersion version = KernelVersion("5.10.25-linuxkit", "");
  EXPECT_EQ(5, version.major);
  EXPECT_EQ(10, version.minor);
  EXPECT_EQ(25, version.patch);
  EXPECT_EQ(0, version.build_id);
}

TEST(KernelVersionTest, TestParseWithBuildIDAndAdditional) {
  KernelVersion version = KernelVersion("3.10.0-957.10.1.el7.x86_64", "");
  EXPECT_EQ(3, version.major);
  EXPECT_EQ(10, version.minor);
  EXPECT_EQ(0, version.patch);
  EXPECT_EQ(957, version.build_id);
}

TEST(KernelVersionTest, TestParseInvalidRelease) {
  KernelVersion version = KernelVersion("not.a.release", "");
  EXPECT_EQ(0, version.major);
  EXPECT_EQ(0, version.minor);
  EXPECT_EQ(0, version.patch);
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
    EXPECT_EQ(k.major, version.major);
    EXPECT_EQ(k.minor, version.minor);
    EXPECT_EQ(k.patch, version.patch);
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

}  // namespace collector
