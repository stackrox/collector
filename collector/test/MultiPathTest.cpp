#include "MultiPath.h"

#include <string>

#include "absl/strings/str_replace.h"
#include "gtest/gtest.h"

namespace {

TEST(MultiPath, TestResolve) {
  using namespace collector;

  struct Case {
    std::string spec;
    std::string expected;
    bool expectError;
  };

  // This is a hack to ensure test execution works from within CLion. If run via CTest, the `make test`
  // target will set the correct working directory.
  if (std::string(std::getenv("PWD")) == "/tmp/cmake-build") {
    chdir("../test");
  }

  Case cases[] = {
          {"testdata/multipath/*/*.txt",                                     "testdata/multipath/b/002-baz.txt", false},
          {"testdata/multipath/*/*-foo.txt",                                 "testdata/multipath/a/002-foo.txt", false},
          {"testdata/multipath/a/*.txt; testdata/multipath/b/*.txt",         "testdata/multipath/a/003-bar.txt", false},
          {"testdata/multipath/b/*.txt; testdata/multipath/a/*.txt",         "testdata/multipath/b/002-baz.txt", false},
          {"testdata/multipath/a/*-foo.txt; testdata/multipath/b/*-foo.txt", "testdata/multipath/a/002-foo.txt", false},
          {"testdata/multipath/b/*-foo.txt; testdata/multipath/a/*-foo.txt", "testdata/multipath/a/002-foo.txt", false},
          {"testdata/multipath/*/*.go",                                      "",                                      true},
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
