#include "StringView.h"
#include "gtest/gtest.h"

namespace collector {

TEST(StringViewTest, TestFindStringExists) {
  StringView view("aaaabbbbcccc");
  std::string::size_type expected = 8;
  ASSERT_EQ(expected, view.find('c'));
}

TEST(StringViewTest, TestFindStringNotExists) {
  StringView view("aaaabbbbcccc");
  ASSERT_EQ(std::string::npos, view.find('d'));
}

TEST(StringViewTest, TestFindStringExistsFromPos) {
  StringView view("abcdefgh");
  std::string::size_type expected = 5;
  ASSERT_EQ(expected, view.find('f', 4));
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

TEST(StringViewTest, TestSplitView) {
  StringView view("aaaa bbbb cccc dddd");
  std::vector<StringView> splits = view.split_view(' ');
  ASSERT_EQ(4, splits.size());

  std::vector<std::string> expected = {
      "aaaa",
      "bbbb",
      "cccc",
      "dddd",
  };

  for (std::string::size_type i = 0; i < splits.size(); i++) {
    ASSERT_EQ(expected[i], splits[i].str());
  }
}

TEST(StringViewTest, TestSplitStr) {
  StringView view("aaaa bbbb cccc dddd");
  std::vector<std::string> splits = view.split_str(' ');
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
  std::vector<std::string> splits = view.split_str(' ');
  ASSERT_EQ(1, splits.size());
  ASSERT_EQ("aaaa", splits[0]);
}

}  // namespace collector