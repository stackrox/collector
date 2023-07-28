#include <cstring>
#include <sstream>
#include <string>

#include "GetKernelObject.cpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

/*
 * The expected value for this test is the well known hash of an empty string:
 * https://www.di-mgt.com.au/sha_testvectors.html
 */
TEST(Sha256HashStream, Empty) {
  std::stringstream input("");
  std::string expected = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
  std::string output = collector::Sha256HashStream(input);

  ASSERT_EQ(output, expected);
}

/*
 * The expected value for this test was verified with the following bash commands:
 * $ printf '0123456789ABCDEF' > newfile
 * $ sha256sum newfile
 */
TEST(Sha256HashStream, SmallString) {
  std::stringstream input("0123456789ABCDEF");
  std::string expected = "2125b2c332b1113aae9bfc5e9f7e3b4c91d828cb942c2df1eeb02502eccae9e9";
  std::string output = collector::Sha256HashStream(input);

  ASSERT_EQ(output, expected);
}

/*
 * The expected value for this test was verified with the following bash commands:
 * $ yes . | head -n 10000 | tr -d "\n" > newfile
 * $ sha256sum newfile
 */
TEST(Sha256HashStream, BigString) {
  std::stringstream input{std::string(10000, '.')};
  std::string expected = "cca2be86bf2ea72443a30bb7b7733dfd09689f975dd8bc807344f32eff401404";
  std::string output = collector::Sha256HashStream(input);

  ASSERT_EQ(output, expected);
}
