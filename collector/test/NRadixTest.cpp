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

#include "NRadix.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace collector {

namespace {

TEST(NRadixTest, TestInsert) {
  NRadixTree tree;
  //// IPv4
  auto inserted = tree.Insert(IPNet(Address(10, 0, 0, 0), 8));
  EXPECT_TRUE(inserted);

  inserted = tree.Insert(IPNet(Address(10, 10, 0, 0), 16));
  EXPECT_TRUE(inserted);

  inserted = tree.Insert(IPNet(Address(10, 0, 0, 0), 16));
  EXPECT_TRUE(inserted);

  inserted = tree.Insert(IPNet(Address(10, 0, 0, 0), 16));
  EXPECT_FALSE(inserted);

  inserted = tree.Insert(IPNet(Address(10, 0, 0, 0), 32));
  EXPECT_TRUE(inserted);

  std::vector<IPNet> expected = std::vector<IPNet>{
    IPNet(Address(10, 0, 0, 0), 32),
    IPNet(Address(10, 10, 0, 0), 16),
    IPNet(Address(10, 0, 0, 0), 16),
    IPNet(Address(10, 0, 0, 0), 8)
  };
  std::vector<IPNet> actual = tree.GetAll();
  std::sort(actual.begin(), actual.end(), std::greater<IPNet>());
  EXPECT_EQ(actual, expected);

  //// IPv6
  inserted = tree.Insert(IPNet(Address(htonll(0x0ULL), htonll(0x10000000000000)), 33));
  EXPECT_TRUE(inserted);

  inserted = tree.Insert(IPNet(Address(htonll(0x2001db833334444), htonll(0x5555666677778888)), 63));
  EXPECT_TRUE(inserted);

  inserted = tree.Insert(IPNet(Address(htonll(0x2001db833334444), htonll(0x5555666677778888)), 64));
  EXPECT_TRUE(inserted);

  inserted = tree.Insert(IPNet(Address(htonll(0x2001db833334444), htonll(0x5555666677778888)), 64));
  EXPECT_FALSE(inserted);

  inserted = tree.Insert(IPNet(Address(htonll(0x2001db833334444), htonll(0x0ULL)), 64));
  EXPECT_FALSE(inserted);

  inserted = tree.Insert(IPNet(Address(htonll(0x2001db833334444), htonll(0x0ULL)), 65));
  EXPECT_TRUE(inserted);

  inserted = tree.Insert(IPNet(Address(htonll(0x2001db833334444), htonll(0x0ULL)), 128));
  EXPECT_TRUE(inserted);

  inserted = tree.Insert(IPNet(Address(htonll(0x2001db833334444), htonll(0x0ULL)), 128));
  EXPECT_FALSE(inserted);

  expected = std::vector<IPNet>{
    IPNet(Address(htonll(0x2001db833334444), htonll(0x0ULL)), 128),
    IPNet(Address(htonll(0x2001db833334444), htonll(0x0ULL)), 65),
    IPNet(Address(htonll(0x2001db833334444), htonll(0x5555666677778888)), 64),
    IPNet(Address(htonll(0x2001db833334444), htonll(0x5555666677778888)), 63),
    IPNet(Address(htonll(0x0ULL), htonll(0x10000000000000)), 33),
    IPNet(Address(10, 0, 0, 0), 32),
    IPNet(Address(10, 10, 0, 0), 16),
    IPNet(Address(10, 0, 0, 0), 16),
    IPNet(Address(10, 0, 0, 0), 8)
  };
  actual = tree.GetAll();
  std::sort(actual.begin(), actual.end(), std::greater<IPNet>());
  EXPECT_EQ(actual, expected);
}

TEST(NRadixTest, TestFind) {
  NRadixTree tree(std::vector<IPNet>{
    IPNet(Address(10, 0, 0, 0), 8),
    IPNet(Address(10, 10, 0, 0), 16),
    IPNet(Address(10, 0, 0, 0), 16),
    IPNet(Address(htonll(0x2001db833334444), htonll(0x5555666677778888)), 63),
    IPNet(Address(htonll(0x2001db833334444), htonll(0x5555666677778888)), 64),
    IPNet(Address(htonll(0x2001db833334444), htonll(0x0ULL)), 70),
    IPNet(Address(htonll(0x2001db833334444), htonll(0x0ULL)), 128),
    });

  //// IPV4 address lookup
  auto actual = tree.Find(Address(10, 10, 10, 10));
  EXPECT_EQ(IPNet(Address(10, 10, 0, 0), 16), actual);

  actual = tree.Find(Address(10, 192, 10, 10));
  EXPECT_EQ(IPNet(Address(10, 0, 0, 0), 8), actual);

  actual = tree.Find(Address(10, 0, 10, 10));
  EXPECT_EQ(IPNet(Address(10, 0, 0, 0), 16), actual);

  actual = tree.Find(Address(1, 0, 10, 10));
  EXPECT_EQ(IPNet(), actual);

  //// IPV4 network lookup
  actual = tree.Find(IPNet(Address(10, 10, 10, 10), 24));
  EXPECT_EQ(IPNet(Address(10, 10, 0, 0), 16), actual);

  actual = tree.Find(IPNet(Address(10, 192, 10, 10), 16));
  EXPECT_EQ(IPNet(Address(10, 0, 0, 0), 8), actual);

  actual = tree.Find(IPNet(Address(10, 0, 10, 10), 32));
  EXPECT_EQ(IPNet(Address(10, 0, 0, 0), 16), actual);

  actual = tree.Find(IPNet(Address(1, 0, 10, 10), 32));
  EXPECT_EQ(IPNet(), actual);

  //// IPV6 address lookup
  actual = tree.Find(Address(10, 0, 0, 0).ToV6());
  EXPECT_EQ(IPNet(), actual);

  actual = tree.Find(Address(htonll(0x0A0A0A0A0A0A0A0AULL), htonll(0x0A0A0A0A0A0A0A0AULL)));
  EXPECT_EQ(IPNet(), actual);

  // Simulate path traversal overlapping with 10.0.0.0/8
  tree.Insert(IPNet(Address(htonll(0x0A0A000000000000ULL), htonll(0ULL)), 80));
  actual = tree.Find(Address(htonll(0x0A0A000000000000ULL), htonll(0x0000000000000A0AULL)));
  EXPECT_EQ(IPNet(Address(htonll(0x0A0A000000000000ULL), htonll(0LL)), 80), actual);

  tree.Insert(IPNet(Address(10, 0, 0, 0).ToV6(), 70));
  actual = tree.Find(Address(10, 0, 0, 0).ToV6());
  EXPECT_EQ(IPNet(Address(10, 0, 0, 0).ToV6(), 70), actual);

  actual = tree.Find(IPNet(Address(htonll(0x2001db833334444), htonll(0x5555666677778888))));
  EXPECT_EQ(IPNet(Address(htonll(0x2001db833334444), htonll(0x0ULL)), 64), actual);

  tree.Insert(IPNet(Address(htonll(0x2001db833334444), htonll(0x5555666677778888))));
  actual = tree.Find(IPNet(Address(htonll(0x2001db833334444), htonll(0x5555666677778888))));
  EXPECT_EQ(IPNet(Address(htonll(0x2001db833334444), htonll(0x5555666677778888))), actual);
}

} // namespace

} // namespace collector
