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
