
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