#include <gmock/gmock-actions.h>
#include <gmock/gmock-spec-builders.h>

#include "Utility.cpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using namespace testing;

namespace collector {

TEST(SplitStringViewTest, TestSplitStr) {
  std::string_view view("aaaa bbbb cccc dddd");
  std::vector<std::string> splits = SplitStringView(view, ' ');
  ASSERT_EQ(4, splits.size());

  std::vector<std::string> expected = {
      "aaaa",
      "bbbb",
      "cccc",
      "dddd",
  };

  for (std::string::size_type i = 0; i < splits.size(); i++) {
    ASSERT_EQ(expected[i], splits[i]);
  }
}

TEST(SplitStringViewTest, TestSplitStrNoDelimiter) {
  std::string_view view("aaaa");
  std::vector<std::string> splits = SplitStringView(view, ' ');
  ASSERT_EQ(1, splits.size());
  ASSERT_EQ("aaaa", splits[0]);
}

TEST(SplitStringViewTest, TestSplitDelimiterAtEnd) {
  std::string_view view("a b c ");
  std::vector<std::string> expected{
      "a",
      "b",
      "c",
      "",
  };
  std::vector<std::string> splits = SplitStringView(view, ' ');
  ASSERT_EQ(expected, splits);
}

TEST(SplitStringViewTest, TestSplitDoubleDelimiter) {
  std::string_view view("a b  c");
  std::vector<std::string> expected{
      "a",
      "b",
      "",
      "c",
  };
  std::vector<std::string> splits = SplitStringView(view, ' ');
  ASSERT_EQ(expected, splits);
}
}  // namespace collector
