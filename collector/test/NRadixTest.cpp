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

#include <random>

#include "Containers.h"
#include "NRadix.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace collector {

namespace {
/*
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

std::pair<std::chrono::duration<double, std::milli>, std::chrono::duration<double, std::milli>> TestLookup(const NRadixTree& tree, const std::vector<IPNet>& networks, Address lookup_addr) {
  auto t1 = std::chrono::steady_clock::now();
  IPNet actual = tree.Find(lookup_addr);
  auto t2 = std::chrono::steady_clock::now();

  EXPECT_NE(IPNet(), actual);
  std::chrono::duration<double, std::milli> tree_lookup_dur = t2 - t1;
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

  EXPECT_NE(IPNet(), actual);
  std::chrono::duration<double, std::milli> linear_lookup_dur = t2 - t1;
  return {tree_lookup_dur, linear_lookup_dur};
}

TEST(NRadixTest, BenchMarkNetworkLookup) {
  size_t num_ipv4_nets = 20000, num_ipv6_nets = 10000;
  size_t num_nets = num_ipv4_nets + num_ipv6_nets;

  std::random_device ip_rd;
  std::default_random_engine ip_gen(ip_rd());
  std::uniform_int_distribution<unsigned long> ip_distr(0, 0xFFFFFFF0);

  std::random_device mask_rd;
  std::default_random_engine mask_gen(mask_rd());
  std::uniform_int_distribution<unsigned char> mask_distr(0x01, 0x20);

  // Generate IPv4 networks.
  UnorderedSet<IPNet> ipv4_network_set;
  ipv4_network_set.reserve(num_ipv4_nets);
  for (int i = 0; i < num_ipv4_nets; i++) {
    ipv4_network_set.insert(IPNet(Address(ip_distr(ip_gen)), mask_distr(mask_gen)));
  }

  // Generate IPv6 networks.
  mask_distr = std::uniform_int_distribution<unsigned char>(0x21, 0x80);
  UnorderedSet<IPNet> ipv6_network_set;
  ipv6_network_set.reserve(num_ipv6_nets);
  for (int i = 0; i < num_ipv6_nets; i++) {
    ipv6_network_set.insert(IPNet(Address(ip_distr(ip_gen), ip_distr(ip_gen)), mask_distr(mask_gen)));
  }

  // Benchmark tree creation.
  auto t1 = std::chrono::steady_clock::now();
  NRadixTree tree;
  for (const auto net : ipv4_network_set) {
    tree.Insert(net);
  }
  for (const auto net : ipv6_network_set) {
    tree.Insert(net);
  }
  auto t2 = std::chrono::steady_clock::now();
  std::chrono::duration<double, std::milli> dur = t2 - t1;
  std::cout << "Time to create tree with " << num_nets << " networks: " << dur.count() << "ms\n";

  // This is from the legacy lookup code. The networks were sorted after receiving from Sensor.
  std::vector<IPNet> ipv4_nets(ipv4_network_set.begin(), ipv4_network_set.end());
  t1 = std::chrono::steady_clock::now();
  std::sort(ipv4_nets.begin(), ipv4_nets.end(), std::greater<IPNet>());
  t2 = std::chrono::steady_clock::now();
  dur = t2 - t1;
  std::cout << "Time to sort " << num_ipv4_nets << " ipv4 networks: " << dur.count() << "ms\n";

  // This is from the legacy lookup code. The networks were sorted after receiving from Sensor.
  std::vector<IPNet> ipv6_nets(ipv6_network_set.begin(), ipv6_network_set.end());
  t1 = std::chrono::steady_clock::now();
  std::sort(ipv6_nets.begin(), ipv6_nets.end(), std::greater<IPNet>());
  t2 = std::chrono::steady_clock::now();
  dur = t2 - t1;
  std::cout << "Time to sort " << num_ipv6_nets << " ipv6 networks: " << dur.count() << "ms\n";

  // Benchmark network lookups.
  // In legacy code, IPv4 and IPv6 networks were stored separately in respective family bucket.
  double aggr_dur_with_tree = 0, aggr_dur_without_tree = 0;
  for (const auto net : ipv4_network_set) {
    const auto durs = TestLookup(tree, ipv4_nets, net.address());
    aggr_dur_with_tree += durs.first.count();
    aggr_dur_without_tree += durs.second.count();
  }

  for (const auto net : ipv6_network_set) {
    const auto durs = TestLookup(tree, ipv6_nets, net.address());
    aggr_dur_with_tree += durs.first.count();
    aggr_dur_without_tree += durs.second.count();
  }

  std::cout << "Avg time to lookup " << num_nets << " addresses with network radix tree (#networks:" << num_nets << "): " << (aggr_dur_with_tree / num_nets) << "ms\n";
  std::cout << "Avg time to lookup " << num_nets << " addresses without network radix tree (#networks:" << num_nets << "): " << (aggr_dur_without_tree / num_nets) << "ms\n";
}
*/
}  // namespace

}  // namespace collector
