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

#include "Containers.h"

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
      IPNet(Address(10, 0, 0, 0), 8)};
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
      IPNet(Address(10, 0, 0, 0), 8)};
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

TEST(NRadixTest, TestIsAnyIPNetSubset) {
  NRadixTree t1(std::vector<IPNet>{
      IPNet(Address(10, 0, 0, 0), 8),
      IPNet(Address(10, 10, 0, 0), 16),
      IPNet(Address(10, 0, 0, 0), 16)});
  NRadixTree t2(std::vector<IPNet>{
      IPNet(Address(11, 0, 0, 0), 8),
      IPNet(Address(11, 10, 0, 0), 16)});
  EXPECT_FALSE(t1.IsAnyIPNetSubset(t2));
  EXPECT_FALSE(t2.IsAnyIPNetSubset(t1));

  t1 = NRadixTree(std::vector<IPNet>{
      IPNet(Address(10, 0, 0, 0), 8),
      IPNet(Address(10, 10, 0, 0), 16),
      IPNet(Address(10, 0, 0, 0), 16)});
  t2 = NRadixTree(std::vector<IPNet>{
      IPNet(Address(11, 0, 0, 0), 8),
      IPNet(Address(11, 10, 0, 0), 16),
      IPNet(Address(10, 1, 0, 0), 16)});
  // 10.0.0.0/8 contains 10.1.0.0/16
  EXPECT_TRUE(t1.IsAnyIPNetSubset(t2));
  EXPECT_FALSE(t2.IsAnyIPNetSubset(t1));

  t1 = NRadixTree(std::vector<IPNet>{
      IPNet(Address(10, 0, 0, 0), 8),
      IPNet(Address(10, 10, 0, 0), 24),
      IPNet(Address(10, 0, 0, 0), 16)});
  t2 = NRadixTree(std::vector<IPNet>{
      IPNet(Address(11, 0, 0, 0), 8),
      IPNet(Address(10, 10, 0, 0), 16),
      IPNet(Address(10, 1, 0, 0), 16)});
  // 10.0.0.0/8 contains 10.1.0.0/16 and 10.10.0.0/16
  EXPECT_TRUE(t1.IsAnyIPNetSubset(t2));
  // 10.10.0.0/16 contains 10.10.0.0/24
  EXPECT_TRUE(t2.IsAnyIPNetSubset(t1));

  t1 = NRadixTree(std::vector<IPNet>{
      IPNet(Address(10, 0, 0, 0), 8),
      IPNet(Address(10, 10, 0, 0), 16),
      IPNet(Address(10, 0, 0, 0), 16),
  });
  // Simulate path traversal overlapping with 10.0.0.0/8
  t2 = NRadixTree(std::vector<IPNet>{
      IPNet(Address(htonll(0x0A0A000000000000ULL), htonll(0ULL)), 80)});
  EXPECT_FALSE(t1.IsAnyIPNetSubset(t2));
  EXPECT_FALSE(t2.IsAnyIPNetSubset(t1));

  t1 = NRadixTree(std::vector<IPNet>{
      IPNet(Address(htonll(0x0A0A000000000000ULL), htonll(0ULL)), 70)});
  t2 = NRadixTree(std::vector<IPNet>{
      IPNet(Address(htonll(0x0A0A000000000000ULL), htonll(0ULL)), 80)});
  EXPECT_TRUE(t1.IsAnyIPNetSubset(t2));
  EXPECT_FALSE(t2.IsAnyIPNetSubset(t1));

  t1 = NRadixTree(std::vector<IPNet>{
      IPNet(Address(htonll(0x0A0A000000000000ULL), htonll(0ULL)), 80)});
  t2 = NRadixTree(std::vector<IPNet>{
      IPNet(Address(htonll(0x0A0A000000000000ULL), htonll(0ULL)), 80)});
  EXPECT_TRUE(t1.IsAnyIPNetSubset(t2));
  EXPECT_TRUE(t2.IsAnyIPNetSubset(t1));

  t1 = NRadixTree(std::vector<IPNet>{
      IPNet(Address(htonll(0x0A0A000000000000ULL), htonll(0ULL)), 16)});
  t2 = NRadixTree(std::vector<IPNet>{
      IPNet(Address(htonll(0x0A0A000000000000ULL), htonll(0ULL)), 80)});
  EXPECT_TRUE(t1.IsAnyIPNetSubset(t2));
  EXPECT_FALSE(t2.IsAnyIPNetSubset(t1));

  t1 = NRadixTree(std::vector<IPNet>{
      IPNet(Address(htonll(0x0A0A000000000000ULL), htonll(0ULL)), 8)});
  t2 = NRadixTree(std::vector<IPNet>{
      IPNet(Address(htonll(0x0A0A000000000000ULL), htonll(0ULL)), 80)});
  EXPECT_TRUE(t1.IsAnyIPNetSubset(t2));
  EXPECT_FALSE(t2.IsAnyIPNetSubset(t1));
}

TEST(NRadixTest, TestIsAnyIPNetSubsetWithFamily) {
  NRadixTree t1(std::vector<IPNet>{
      IPNet(Address(10, 0, 0, 0), 8),
      IPNet(Address(10, 10, 0, 0), 16),
      IPNet(Address(10, 0, 0, 0), 16)});
  NRadixTree t2(std::vector<IPNet>{
      IPNet(Address(11, 0, 0, 0), 8),
      IPNet(Address(11, 10, 0, 0), 16)});
  EXPECT_FALSE(t1.IsAnyIPNetSubset(Address::Family::IPV4, t2));
  EXPECT_FALSE(t2.IsAnyIPNetSubset(Address::Family::IPV4, t1));

  t1 = NRadixTree(std::vector<IPNet>{
      IPNet(Address(10, 0, 0, 0), 8),
      IPNet(Address(10, 10, 0, 0), 16),
      IPNet(Address(10, 0, 0, 0), 16)});
  t2 = NRadixTree(std::vector<IPNet>{
      IPNet(Address(11, 0, 0, 0), 8),
      IPNet(Address(11, 10, 0, 0), 16),
      IPNet(Address(10, 1, 0, 0), 16)});
  // 10.0.0.0/8 contains 10.1.0.0/16
  EXPECT_TRUE(t1.IsAnyIPNetSubset(Address::Family::IPV4, t2));

  t1 = NRadixTree(std::vector<IPNet>{
      IPNet(Address(10, 0, 0, 0), 8),
      IPNet(Address(10, 10, 0, 0), 16),
      IPNet(Address(10, 0, 0, 0), 16)});
  t2 = NRadixTree(std::vector<IPNet>{
      IPNet(Address(11, 0, 0, 0), 8),
      IPNet(Address(11, 10, 0, 0), 16),
      IPNet(Address(10, 1, 0, 0), 16)});
  // 10.0.0.0/8 contains 10.1.0.0/16 but we are checking IPv6 private subnet existence.
  EXPECT_FALSE(t1.IsAnyIPNetSubset(Address::Family::IPV6, t2));

  t1 = NRadixTree(std::vector<IPNet>{
      IPNet(Address(10, 0, 0, 0), 8),
      IPNet(Address(10, 10, 0, 0), 24),
      IPNet(Address(10, 0, 0, 0), 16)});
  t2 = NRadixTree(std::vector<IPNet>{
      IPNet(Address(11, 0, 0, 0), 8),
      IPNet(Address(10, 10, 0, 0), 16),
      IPNet(Address(10, 1, 0, 0), 16)});
  // 10.0.0.0/8 contains 10.1.0.0/16 and 10.10.0.0/16
  EXPECT_TRUE(t1.IsAnyIPNetSubset(Address::Family::IPV4, t2));
  // 10.10.0.0/16 contains 10.10.0.0/24
  EXPECT_TRUE(t2.IsAnyIPNetSubset(Address::Family::IPV4, t1));

  t1 = NRadixTree(std::vector<IPNet>{
      IPNet(Address(10, 0, 0, 0), 8),
      IPNet(Address(10, 10, 0, 0), 24),
      IPNet(Address(10, 0, 0, 0), 16)});
  t2 = NRadixTree(std::vector<IPNet>{
      IPNet(Address(11, 0, 0, 0), 8),
      IPNet(Address(10, 10, 0, 0), 16),
      IPNet(Address(10, 1, 0, 0), 16)});
  // 10.0.0.0/8 contains 10.1.0.0/16 and 10.10.0.0/16 but we are checking for IPv6 private subnet existence.
  EXPECT_FALSE(t1.IsAnyIPNetSubset(Address::Family::IPV6, t2));
  // 10.10.0.0/16 contains 10.10.0.0/24 but we are checking for IPv6 private subnet existence.
  EXPECT_FALSE(t2.IsAnyIPNetSubset(Address::Family::IPV6, t1));
}

void TestLookup(const NRadixTree& tree, const std::vector<IPNet>& networks, Address lookup_addr) {
  auto t1 = std::chrono::steady_clock::now();
  IPNet actual = tree.Find(lookup_addr);
  auto t2 = std::chrono::steady_clock::now();

  EXPECT_EQ(IPNet(lookup_addr), actual);
  std::chrono::duration<double, std::milli> tree_lookup_dur = t2 - t1;
  std::cout << "Address (" << lookup_addr << ") lookup time with network radix tree: " << tree_lookup_dur.count() << "ms\n";
  actual = {};

  t1 = std::chrono::steady_clock::now();
  // This is the legacy lookup code.
  for (const auto net : networks) {
    if (net.Contains(lookup_addr)) {
      actual = net;
      break;
    }
  }
  t2 = std::chrono::steady_clock::now();

  EXPECT_EQ(IPNet(lookup_addr), actual);
  std::chrono::duration<double, std::milli> linear_lookup_dur = t2 - t1;
  std::cout << "Address (" << lookup_addr << ") lookup time without network radix tree: " << linear_lookup_dur.count() << "ms\n";
}

TEST(NRadixTest, BenchMarkNetworkLookup) {
  std::vector<IPNet> nets;
  nets.reserve(20000);

  IPNet net(Address(1, 1, 1, 1));
  nets.push_back(net);

  // Last addr is 1.1.79.33
  for (int i = 1; i < 20000; i++) {
    const uint32_t next_ip = ntohl(static_cast<uint32_t>(*net.address().u64_data())) + 1UL;
    net = IPNet(Address(htonl(next_ip)));
    nets.emplace_back(net);
  }

  auto t1 = std::chrono::steady_clock::now();
  NRadixTree tree(nets);
  auto t2 = std::chrono::steady_clock::now();
  std::chrono::duration<double, std::milli> dur = t2 - t1;
  std::cout << "Time to create tree with 20000 networks: " << dur.count() << "ms\n";

  // This is from the legacy lookup code. The networks were sorted after receiving from Sensor.
  // Therefore the vector ends up being 1.1.79.32, 1.1.79.31 ... 1.1.1.1.
  t1 = std::chrono::steady_clock::now();
  std::sort(nets.begin(), nets.end(), std::greater<IPNet>());
  t2 = std::chrono::steady_clock::now();
  dur = t2 - t1;
  std::cout << "Time to sort 20000 networks: " << dur.count() << "ms\n";

  TestLookup(tree, nets, Address(1, 1, 79, 32));
  TestLookup(tree, nets, Address(1, 1, 79, 1));
  TestLookup(tree, nets, Address(1, 1, 50, 100));
  TestLookup(tree, nets, Address(1, 1, 40, 100));
  TestLookup(tree, nets, Address(1, 1, 30, 100));
  TestLookup(tree, nets, Address(1, 1, 20, 100));
  TestLookup(tree, nets, Address(1, 1, 10, 100));
  TestLookup(tree, nets, Address(1, 1, 1, 1));
}

} // namespace

}  // namespace collector
