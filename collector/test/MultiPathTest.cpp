#include "MultiPath.h"

#include <string>

#include "absl/strings/str_replace.h"
#include "absl/flags/flag.h"
#include "gtest/gtest.h"

ABSL_FLAG(std::string, testdata_dir, "/tmp/test/testdata", "Location of test data");

namespace {

TEST(MultiPath, TestResolve) {
  using namespace collector;

  struct Case {
    std::string spec;
    std::string expected;
    bool expectError;
  };

  Case cases[] = {
          {"$d/multipath/*/*.txt",                                          "$d/multipath/b/002-baz.txt", false},
          {"$d/multipath/*/*-foo.txt",                                      "$d/multipath/a/002-foo.txt", false},
          {"$d/multipath/a/*.txt; $d/multipath/b/*.txt",         "$d/multipath/a/003-bar.txt", false},
          {"$d/multipath/b/*.txt; $d/multipath/a/*.txt",         "$d/multipath/b/002-baz.txt", false},
          {"$d/multipath/a/*-foo.txt; $d/multipath/b/*-foo.txt", "$d/multipath/a/002-foo.txt", false},
          {"$d/multipath/b/*-foo.txt; $d/multipath/a/*-foo.txt", "$d/multipath/a/002-foo.txt", false},
          {"$d/multipath/*/*.go",                                           "",                                      true},
  };

  for (const auto &c : cases) {
    bool ok;
    auto spec = absl::StrReplaceAll(c.spec, {{"$d", absl::GetFlag(FLAGS_testdata_dir)}});
    auto expected = absl::StrReplaceAll(c.expected, {{"$d", absl::GetFlag(FLAGS_testdata_dir)}});

    auto resolved = ResolveMultiPath(spec, &ok);

    if (c.expectError) {
      ASSERT_FALSE(ok);
      ASSERT_EQ(spec, resolved);
    } else {
      ASSERT_TRUE(ok);
      ASSERT_EQ(expected, resolved);
    }
  }
}

}  // namespace
