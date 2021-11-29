#include "StringView.h"
#include "gtest/gtest.h"

namespace collector {

TEST(StringViewTest, TestFindStringExists) {
  StringView view("aaaabbbbcccc");
  StringView::size_type expected = 8;
  ASSERT_EQ(expected, view.find('c'));
}

TEST(StringViewTest, TestFindStringNotExists) {
  StringView view("aaaabbbbcccc");
  ASSERT_EQ(std::string::npos, view.find('d'));
}

TEST(StringViewTest, TestFindStringExistsFromPos) {
  StringView view("This f is skipped, but this f is not.");
  StringView::size_type expected = 28;
  ASSERT_EQ(expected, view.find('f', 10));
}

TEST(StringViewTest, TestFindStringNotExistsFromPos) {
  StringView view("aaaabbbbcccc");
  ASSERT_EQ(std::string::npos, view.find('d', 5));
}

TEST(StringViewTest, TestFindOffsetTooLarge) {
  StringView view("aaaa");
  ASSERT_EQ(std::string::npos, view.find('a', 10));
}

TEST(StringViewTest, TestSubstr) {
  StringView view("aaaabbbb");
  ASSERT_EQ("bbbb", view.substr(4).str());
}

TEST(StringViewTest, TestSubstrWithCount) {
  StringView view("aaaabbbb");
  ASSERT_EQ("ab", view.substr(3, 2).str());
}

TEST(StringViewTest, TestSubstrCountTooLarge) {
  StringView view("aaaabb");
  ASSERT_EQ("aabb", view.substr(2, 10).str());
}

TEST(StringViewTest, TestSubstrPosTooLarge) {
  StringView view("aaaa");
  ASSERT_EQ("", view.substr(100).str());
}

TEST(StringViewTest, TestSplitStr) {
  StringView view("aaaa bbbb cccc dddd");
  std::vector<std::string> splits = view.split(' ');
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

TEST(StringViewTest, TestSplitStrNoDelimiter) {
  StringView view("aaaa");
  std::vector<std::string> splits = view.split(' ');
  ASSERT_EQ(1, splits.size());
  ASSERT_EQ("aaaa", splits[0]);
}

TEST(StringViewTest, TestSplitDelimiterAtEnd) {
  StringView view("a b c ");
  std::vector<std::string> expected{
      "a",
      "b",
      "c",
      "",
  };
  std::vector<std::string> splits = view.split(' ');
  ASSERT_EQ(expected, splits);
}

TEST(StringViewTest, TestSplitDoubleDelimiter) {
  StringView view("a b  c");
  std::vector<std::string> expected{
      "a",
      "b",
      "",
      "c",
  };
  std::vector<std::string> splits = view.split(' ');
  ASSERT_EQ(expected, splits);
}

}  // namespace collector
