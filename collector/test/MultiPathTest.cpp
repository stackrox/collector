#include "MultiPath.h"

#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

TEST(MultiPath, TestResolve) {
  using namespace collector;

  struct Case {
    std::string spec;
    std::string expected;
    bool expectError;
  };

  Case cases[] = {
          {"test/testdata/multipath/*/*.txt",                                          "test/testdata/multipath/b/002-baz.txt", false},
          {"test/testdata/multipath/*/*-foo.txt",                                      "test/testdata/multipath/a/002-foo.txt", false},
          {"test/testdata/multipath/a/*.txt; test/testdata/multipath/b/*.txt",         "test/testdata/multipath/a/003-bar.txt", false},
          {"test/testdata/multipath/b/*.txt; test/testdata/multipath/a/*.txt",         "test/testdata/multipath/b/002-baz.txt", false},
          {"test/testdata/multipath/a/*-foo.txt; test/testdata/multipath/b/*-foo.txt", "test/testdata/multipath/a/002-foo.txt", false},
          {"test/testdata/multipath/b/*-foo.txt; test/testdata/multipath/a/*-foo.txt", "test/testdata/multipath/a/002-foo.txt", false},
          {"test/testdata/multipath/*/*.go",                                           "",                                      true},
  };

  for (const auto &c : cases) {
    bool ok;
    auto resolved = ResolveMultiPath(c.spec, &ok);
    if (c.expectError) {
      ASSERT_FALSE(ok);
      ASSERT_EQ(c.spec, resolved);
    } else {
      ASSERT_TRUE(ok);
      ASSERT_EQ(c.expected, resolved);
    }
  }
}

}  // namespace
