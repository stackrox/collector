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

#include "NetworkConnection.h"

#include <utility>

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "Utility.h"

namespace collector {

namespace {

TEST(TestAddress, TestConstructors) {
  Address a(192, 168, 0, 1);
  EXPECT_EQ(Str(a), "192.168.0.1");
  EXPECT_EQ(a.array()[0], 0xc0a8000100000000ULL);
  uint8_t a_bytes[4] = {192, 168, 0, 1};
  EXPECT_EQ(a.length(), 4);
  EXPECT_EQ(std::memcmp(a.network_array().data(), a_bytes, a.length()), 0);
  EXPECT_FALSE(a.IsPublic());

  Address b(htonl(0x7f000001));
  EXPECT_EQ(Str(b), "127.0.0.1");
  EXPECT_EQ(b.array()[0], 0x7f00000100000000ULL);
  uint8_t b_bytes[4] = {127, 0, 0, 1};
  EXPECT_EQ(a.length(), 4);
  EXPECT_EQ(std::memcmp(b.network_array().data(), b_bytes, b.length()), 0);
  EXPECT_TRUE(b.IsPublic());

  uint32_t ipv6_data[4] = {htonl(0x2a020908), htonl(0xe850cf20), htonl(0x991944af), htonl(0xa46e1669)};
  Address c(ipv6_data);
  EXPECT_EQ(Str(c), "2a02:908:e850:cf20:9919:44af:a46e:1669");
  EXPECT_EQ(c.array()[0], 0x2a020908e850cf20ULL);
  EXPECT_EQ(c.array()[1], 0x991944afa46e1669ULL);
  uint8_t c_bytes[16] = {0x2a, 0x02, 0x09, 0x08, 0xe8, 0x50, 0xcf, 0x20, 0x99, 0x19, 0x44, 0xaf, 0xa4, 0x6e, 0x16, 0x69};
  EXPECT_EQ(std::memcmp(c.network_array().data(), c_bytes, c.length()), 0);
  EXPECT_TRUE(c.IsPublic());
}


TEST(TestAddress, TestConstructors) {
  Address a(172, 217, 212, 95);
  EXPECT_TRUE(a.IsPublic());
}

}  // namespace

}  // namespace collector
