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
  EXPECT_EQ(a.array()[0], htonll(0xc0a8000100000000ULL));
  uint8_t a_bytes[4] = {192, 168, 0, 1};
  EXPECT_EQ(a.length(), 4);
  EXPECT_EQ(std::memcmp(a.data(), a_bytes, a.length()), 0);
  EXPECT_FALSE(a.IsPublic());
  EXPECT_FALSE(a.ToV6().IsPublic());

  Address b(htonl(0x7f000001));
  EXPECT_EQ(Str(b), "127.0.0.1");
  EXPECT_EQ(b.array()[0], htonll(0x7f00000100000000ULL));
  uint8_t b_bytes[4] = {127, 0, 0, 1};
  EXPECT_EQ(a.length(), 4);
  EXPECT_EQ(std::memcmp(b.data(), b_bytes, b.length()), 0);
  EXPECT_TRUE(b.IsPublic());
  EXPECT_TRUE(b.ToV6().IsPublic());

  uint32_t ipv6_data[4] = {htonl(0x2a020908), htonl(0xe850cf20), htonl(0x991944af), htonl(0xa46e1669)};
  Address c(ipv6_data);
  EXPECT_EQ(Str(c), "2a02:908:e850:cf20:9919:44af:a46e:1669");
  EXPECT_EQ(c.array()[0], htonll(0x2a020908e850cf20ULL));
  EXPECT_EQ(c.array()[1], htonll(0x991944afa46e1669ULL));
  uint8_t c_bytes[16] = {0x2a, 0x02, 0x09, 0x08, 0xe8, 0x50, 0xcf, 0x20, 0x99, 0x19, 0x44, 0xaf, 0xa4, 0x6e, 0x16, 0x69};
  EXPECT_EQ(std::memcmp(c.data(), c_bytes, c.length()), 0);
  EXPECT_TRUE(c.IsPublic());
  EXPECT_TRUE(c.ToV6().IsPublic());
}


TEST(TestAddress, TestMaskAndIsPublic) {
  IPNet mask(Address(172, 16, 0, 0), 12);
  const auto& mask_data = mask.mask_array();
  EXPECT_EQ(mask.family(), Address::Family::IPV4);
  EXPECT_EQ(mask.bits(), 12);
  EXPECT_EQ(mask_data[0], 0xac10000000000000ULL);

  Address a(172, 217, 212, 95);
  EXPECT_TRUE(a.IsPublic());
  EXPECT_TRUE(a.ToV6().IsPublic());
  EXPECT_FALSE(mask.Contains(a));

  Address b(169, 254, 169, 254);
  EXPECT_FALSE(b.IsPublic());
  EXPECT_FALSE(b.ToV6().IsPublic());
  EXPECT_FALSE(mask.Contains(b));

  Address c(169, 253, 169, 254);
  EXPECT_TRUE(c.IsPublic());
  EXPECT_TRUE(c.ToV6().IsPublic());
  EXPECT_FALSE(mask.Contains(c));

  EXPECT_FALSE(mask.Contains(Address(172, 15, 255, 255)));
  EXPECT_TRUE(mask.Contains(Address(172, 16, 0, 0)));
  EXPECT_TRUE(mask.Contains(Address(172, 31, 255, 255)));
  EXPECT_FALSE(mask.Contains(Address(172, 32, 0, 0)));
}

TEST(TestAddress, TestLoopback) {
  Address a(127, 0, 10, 1);
  EXPECT_TRUE(a.IsLocal());
  EXPECT_TRUE(a.ToV6().IsLocal());

  Address b(192, 168, 0, 1);
  EXPECT_FALSE(b.IsLocal());
  EXPECT_FALSE(b.ToV6().IsLocal());

  Address c(0ULL, htonll(0x1ULL));
  EXPECT_TRUE(c.IsLocal());
}

TEST(TestIPNet, TestNetworkDescComparator) {
  std::array<IPNet, 8> networks = {
    IPNet(Address(10, 0, 0, 0), 8),
    IPNet(Address(127, 0, 0, 1), 16),
    IPNet(Address(127, 254, 0, 0), 16),
    IPNet(Address(172, 16, 0, 0), 12),
    IPNet(Address(192, 0, 0, 0), 16),
    IPNet(Address(192, 0, 0, 0), 8),
    IPNet(Address(192, 0, 0, 1), 8),
    IPNet(Address(200, 200), 8),
  };

  std::array<IPNet, 8> expected = {
    IPNet(Address(192, 0, 0, 0), 16),
    IPNet(Address(127, 254, 0, 0), 16),
    IPNet(Address(127, 0, 0, 1), 16),
    IPNet(Address(172, 16, 0, 0), 12),
    IPNet(Address(200, 200), 8),
    IPNet(Address(192, 0, 0, 1), 8),
    IPNet(Address(192, 0, 0, 0), 8),
    IPNet(Address(10, 64, 0, 0), 8),
  };

  std::sort(networks.begin(), networks.end(), std::greater<IPNet>());

  EXPECT_EQ(networks, expected);
}

}  // namespace

}  // namespace collector
