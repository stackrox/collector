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
