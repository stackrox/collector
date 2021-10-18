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

#include <utility>

#include "ConnTracker.h"
#include "TimeUtil.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace collector {

namespace {

using CT = ConnectionTracker;
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

TEST(ConnTrackerTest, TestAddRemove) {
  Endpoint a(Address(192, 168, 0, 1), 80);
  Endpoint b(Address(192, 168, 1, 10), 9999);

  Connection conn1("xyz", a, b, L4Proto::TCP, true);
  Connection conn2("xzy", b, a, L4Proto::TCP, false);

  int64_t now = NowMicros();

  ConnectionTracker tracker;
  tracker.AddConnection(conn1, now);
  tracker.AddConnection(conn2, now);

  auto state = tracker.FetchConnState();
  EXPECT_THAT(state, UnorderedElementsAre(std::make_pair(conn1, ConnStatus(now, true)), std::make_pair(conn2, ConnStatus(now, true))));

  auto state2 = tracker.FetchConnState();
  EXPECT_EQ(state, state2);

  int64_t now2 = NowMicros();
  tracker.RemoveConnection(conn1, now2);
  state = tracker.FetchConnState();
  EXPECT_THAT(state, UnorderedElementsAre(std::make_pair(conn1, ConnStatus(now2, false)), std::make_pair(conn2, ConnStatus(now, true))));

  state = tracker.FetchConnState();
  EXPECT_THAT(state, UnorderedElementsAre(std::make_pair(conn2, ConnStatus(now, true))));
}

TEST(ConnTrackerTest, TestUpdate) {
  Endpoint a(Address(192, 168, 0, 1), 80);
  Endpoint b(Address(192, 168, 1, 10), 9999);

  Connection conn1("xyz", a, b, L4Proto::TCP, true);
  Connection conn2("xzy", b, a, L4Proto::TCP, false);

  int64_t now = NowMicros();

  ConnectionTracker tracker;
  tracker.Update({conn1, conn2}, {}, now);

  auto state = tracker.FetchConnState();
  EXPECT_THAT(state, UnorderedElementsAre(std::make_pair(conn1, ConnStatus(now, true)), std::make_pair(conn2, ConnStatus(now, true))));

  auto state2 = tracker.FetchConnState();
  EXPECT_EQ(state, state2);

  int64_t now2 = NowMicros();
  tracker.Update({conn1}, {}, now2);
  state = tracker.FetchConnState();
  EXPECT_THAT(state, UnorderedElementsAre(std::make_pair(conn1, ConnStatus(now2, true)), std::make_pair(conn2, ConnStatus(now, false))));

  state = tracker.FetchConnState();
  EXPECT_THAT(state, UnorderedElementsAre(std::make_pair(conn1, ConnStatus(now2, true))));
}

TEST(ConnTrackerTest, TestUpdateIgnoredL4ProtoPortPairs) {
  Endpoint a(Address(192, 168, 0, 1), 80);
  Endpoint b(Address(192, 168, 1, 10), 9999);
  Endpoint e(Address(192, 168, 0, 1), 9);
  Endpoint f(Address(192, 168, 1, 10), 54322);

  Connection conn_ab("xyz", a, b, L4Proto::TCP, true);
  Connection conn_ba("xzy", b, a, L4Proto::TCP, false);
  Connection conn_ef("xyz", e, f, L4Proto::UDP, true);
  Connection conn_fe("xzy", f, e, L4Proto::UDP, false);

  Connection conn_ab_normalized("xyz",
                                Endpoint(IPNet(Address()), 80),
                                Endpoint(IPNet(Address(192, 168, 1, 10), 0, true), 0),
                                L4Proto::TCP, true);
  Connection conn_ba_normalized("xzy",
                                Endpoint(),
                                Endpoint(IPNet(Address(192, 168, 0, 1), 0, true), 80),
                                L4Proto::TCP, false);
  Connection conn_ef_normalized("xyz",
                                Endpoint(IPNet(Address()), 9),
                                Endpoint(IPNet(f.address(), 0, true), 0), L4Proto::UDP, true);
  Connection conn_fe_normalized("xzy",
                                Endpoint(),
                                Endpoint(IPNet(e.address(), 0, true), 9),
                                L4Proto::UDP, false);
  int64_t now = NowMicros();
  ConnectionTracker tracker;
  tracker.Update({conn_ab, conn_ba, conn_ef, conn_fe}, {}, now);

  // no normalization, no l4protoport filtering
  auto state = tracker.FetchConnState(false);
  EXPECT_THAT(state, UnorderedElementsAre(
                         std::make_pair(conn_ab, ConnStatus(now, true)),
                         std::make_pair(conn_ba, ConnStatus(now, true)),
                         std::make_pair(conn_ef, ConnStatus(now, true)),
                         std::make_pair(conn_fe, ConnStatus(now, true))));

  // no normalization, filter out udp/9
  UnorderedSet<L4ProtoPortPair> ignored_proto_port_pairs;
  ignored_proto_port_pairs.insert(L4ProtoPortPair(L4Proto::UDP, 9));
  tracker.UpdateIgnoredL4ProtoPortPairs(std::move(ignored_proto_port_pairs));
  state = tracker.FetchConnState(false);
  EXPECT_THAT(state, UnorderedElementsAre(
                         std::make_pair(conn_ab, ConnStatus(now, true)),
                         std::make_pair(conn_ba, ConnStatus(now, true))));

  //normalization, filter out udp/9
  state = tracker.FetchConnState(true);
  EXPECT_THAT(state, UnorderedElementsAre(
                         std::make_pair(conn_ab_normalized, ConnStatus(now, true)),
                         std::make_pair(conn_ba_normalized, ConnStatus(now, true))));

  // normalization, no l4protoport filtering
  state = tracker.FetchConnState(true);
  tracker.UpdateIgnoredL4ProtoPortPairs(UnorderedSet<L4ProtoPortPair>());
  state = tracker.FetchConnState(true);
  EXPECT_THAT(state, UnorderedElementsAre(
                         std::make_pair(conn_ab_normalized, ConnStatus(now, true)),
                         std::make_pair(conn_ba_normalized, ConnStatus(now, true)),
                         std::make_pair(conn_ef_normalized, ConnStatus(now, true)),
                         std::make_pair(conn_fe_normalized, ConnStatus(now, true))));
}

TEST(ConnTrackerTest, TestUpdateNormalized) {
  Endpoint a(Address(192, 168, 0, 1), 80);
  Endpoint b(Address(192, 168, 1, 10), 9999);
  Endpoint c(Address(192, 168, 1, 10), 54321);
  Endpoint d(Address(35, 127, 0, 15), 54321);

  Connection conn1("xyz", a, b, L4Proto::TCP, true);
  Connection conn2("xzy", b, a, L4Proto::TCP, false);
  Connection conn3("xyz", a, c, L4Proto::TCP, true);
  Connection conn4("xzy", c, a, L4Proto::TCP, false);
  Connection conn5("xyz", a, d, L4Proto::TCP, true);

  Connection conn13_normalized("xyz", Endpoint(IPNet(Address()), 80), Endpoint(IPNet(Address(192, 168, 1, 10), 0, true), 0), L4Proto::TCP, true);
  Connection conn24_normalized("xzy", Endpoint(), Endpoint(IPNet(Address(192, 168, 0, 1), 0, true), 80), L4Proto::TCP, false);

  int64_t now = NowMicros();

  ConnectionTracker tracker;
  tracker.Update({conn1, conn2, conn3, conn4}, {}, now);

  auto state = tracker.FetchConnState(true);
  EXPECT_THAT(state, UnorderedElementsAre(
                         std::make_pair(conn13_normalized, ConnStatus(now, true)),
                         std::make_pair(conn24_normalized, ConnStatus(now, true))));

  auto state2 = tracker.FetchConnState(true);
  EXPECT_EQ(state, state2);

  int64_t now2 = NowMicros();
  tracker.Update({conn1}, {}, now2);
  state = tracker.FetchConnState(true);
  EXPECT_THAT(state, UnorderedElementsAre(
                         std::make_pair(conn13_normalized, ConnStatus(now2, true)),
                         std::make_pair(conn24_normalized, ConnStatus(now, false))));

  state = tracker.FetchConnState(true);
  EXPECT_THAT(state, UnorderedElementsAre(std::make_pair(conn13_normalized, ConnStatus(now2, true))));

  // Private subnet containing the address; exact private IP subnet
  UnorderedMap<Address::Family, std::vector<IPNet>> known_networks = {{Address::Family::IPV4, {IPNet(Address(192, 168, 0, 0), 16)}}};
  tracker.UpdateKnownIPNetworks(std::move(known_networks));

  Connection conn13_1_normalized("xyz", Endpoint(IPNet(), 80), Endpoint(IPNet(Address(192, 168, 1, 10), 16, true), 0), L4Proto::TCP, true);

  state = tracker.FetchConnState(true);
  EXPECT_THAT(state, UnorderedElementsAre(std::make_pair(conn13_1_normalized, ConnStatus(now2, true))));

  // Private subnet containing the address; user-defined contained in private IP space.
  known_networks = {{Address::Family::IPV4, {IPNet(Address(192, 168, 1, 0), 24)}}};
  tracker.UpdateKnownIPNetworks(std::move(known_networks));

  Connection conn13_1_1_normalized("xyz", Endpoint(IPNet(), 80), Endpoint(IPNet(Address(192, 168, 1, 10), 24, true), 0), L4Proto::TCP, true);

  state = tracker.FetchConnState(true);
  EXPECT_THAT(state, UnorderedElementsAre(std::make_pair(conn13_1_1_normalized, ConnStatus(now2, true))));

  // Private subnet containing the address; user-defined contains a private IP space.
  known_networks = {{Address::Family::IPV4, {IPNet(Address(192, 168, 0, 0), 8)}}};
  tracker.UpdateKnownIPNetworks(std::move(known_networks));

  Connection conn13_1_2_normalized("xyz", Endpoint(IPNet(), 80), Endpoint(IPNet(Address(192, 168, 1, 10), 8, true), 0), L4Proto::TCP, true);

  state = tracker.FetchConnState(true);
  EXPECT_THAT(state, UnorderedElementsAre(std::make_pair(conn13_1_2_normalized, ConnStatus(now2, true))));

  // No private subnet
  known_networks = {};
  tracker.UpdateKnownIPNetworks(std::move(known_networks));

  Connection conn13_1_3_normalized("xyz", Endpoint(IPNet(), 80), Endpoint(IPNet(Address(192, 168, 1, 10), 0, true), 0), L4Proto::TCP, true);

  state = tracker.FetchConnState(true);
  EXPECT_THAT(state, UnorderedElementsAre(std::make_pair(conn13_1_3_normalized, ConnStatus(now2, true))));

  // No private subnet; public subnet, private IP
  known_networks = {{Address::Family::IPV4, {IPNet(Address(194, 168, 0, 0), 8)}}};
  tracker.UpdateKnownIPNetworks(std::move(known_networks));

  state = tracker.FetchConnState(true);
  EXPECT_THAT(state, UnorderedElementsAre(std::make_pair(conn13_1_3_normalized, ConnStatus(now2, true))));

  // Single IP address as private subnet
  known_networks = {{Address::Family::IPV4, {IPNet(Address(192, 168, 1, 10), 32)}}};
  tracker.UpdateKnownIPNetworks(std::move(known_networks));

  Connection conn13_2_normalized("xyz", Endpoint(IPNet(), 80), Endpoint(IPNet(Address(192, 168, 1, 10)), 0), L4Proto::TCP, true);

  state = tracker.FetchConnState(true);
  EXPECT_THAT(state, UnorderedElementsAre(std::make_pair(conn13_2_normalized, ConnStatus(now2, true))));

  // Subnet not containing the address
  known_networks = {{Address::Family::IPV4, {IPNet(Address(192, 168, 0, 0), 24)}}};
  tracker.UpdateKnownIPNetworks(std::move(known_networks));

  Connection conn13_3_normalized("xyz", Endpoint(IPNet(), 80), Endpoint(IPNet(Address(192, 168, 1, 10), 0, true), 0), L4Proto::TCP, true);

  state = tracker.FetchConnState(true);
  EXPECT_THAT(state, UnorderedElementsAre(std::make_pair(conn13_3_normalized, ConnStatus(now2, true))));

  state = tracker.FetchConnState(true);
  EXPECT_THAT(state, UnorderedElementsAre(std::make_pair(conn13_3_normalized, ConnStatus(now2, true))));

  // Single IP address as public subnet
  UnorderedSet<Address> public_ips = {Address(35, 127, 0, 15)};
  tracker.UpdateKnownPublicIPs(std::move(public_ips));

  known_networks = {{Address::Family::IPV4, {IPNet(Address(35, 127, 0, 15), 32)}}};
  tracker.UpdateKnownIPNetworks(std::move(known_networks));

  Connection conn15_normalized("xyz", Endpoint(IPNet(), 80), Endpoint(IPNet(Address(35, 127, 0, 15), 32, true), 0), L4Proto::TCP, true);

  int64_t now3 = NowMicros();
  tracker.Update({conn5}, {}, now3);
  state = tracker.FetchConnState(true);
  EXPECT_THAT(state, UnorderedElementsAre(
                         std::make_pair(conn15_normalized, ConnStatus(now3, true)),
                         std::make_pair(conn13_3_normalized, ConnStatus(now2, false))));

  state = tracker.FetchConnState(true);
  EXPECT_THAT(state, UnorderedElementsAre(std::make_pair(conn15_normalized, ConnStatus(now3, true))));

  // No known cluster entities
  public_ips = {};
  tracker.UpdateKnownPublicIPs(std::move(public_ips));
  Connection conn15_1_normalized("xyz", Endpoint(IPNet(), 80), Endpoint(IPNet(Address(35, 127, 0, 15), 32, false), 0), L4Proto::TCP, true);

  state = tracker.FetchConnState(true);
  EXPECT_THAT(state, UnorderedElementsAre(std::make_pair(conn15_1_normalized, ConnStatus(now3, true))));

  // No known networks
  public_ips = {Address(35, 127, 0, 15)};
  tracker.UpdateKnownPublicIPs(std::move(public_ips));

  known_networks = {};
  tracker.UpdateKnownIPNetworks(std::move(known_networks));
  Connection conn15_2_normalized("xyz", Endpoint(IPNet(), 80), Endpoint(IPNet(Address(35, 127, 0, 15), 0, true), 0), L4Proto::TCP, true);

  state = tracker.FetchConnState(true);
  EXPECT_THAT(state, UnorderedElementsAre(std::make_pair(conn15_2_normalized, ConnStatus(now3, true))));

  // private subnet, public IP
  public_ips = {Address(35, 127, 0, 15)};
  tracker.UpdateKnownPublicIPs(std::move(public_ips));

  known_networks = {{Address::Family::IPV4, {IPNet(Address(192, 168, 0, 0), 8)}}};
  tracker.UpdateKnownIPNetworks(std::move(known_networks));

  state = tracker.FetchConnState(true);
  EXPECT_THAT(state, UnorderedElementsAre(std::make_pair(conn15_2_normalized, ConnStatus(now3, true))));
}

TEST(ConnTrackerTest, TestUpdateNormalizedExternal) {
  Endpoint a(Address(10, 1, 1, 8), 9999);
  Endpoint b(Address(35, 127, 0, 15), 54321);
  Endpoint c(Address(139, 14, 171, 3), 54321);
  Endpoint d(Address(35, 127, 1, 200), 54321);

  Connection conn1("xyz", a, b, L4Proto::TCP, true);
  Connection conn2("xyz", a, b, L4Proto::TCP, false);
  Connection conn3("xyz", a, c, L4Proto::TCP, true);
  Connection conn4("xyz", a, c, L4Proto::TCP, false);
  Connection conn5("xyz", a, d, L4Proto::TCP, true);

  Connection conn13_normalized("xyz", Endpoint(IPNet(), 9999), Endpoint(IPNet(Address(255, 255, 255, 255), 0, true), 0), L4Proto::TCP, true);
  Connection conn24_normalized("xyz", Endpoint(), Endpoint(IPNet(Address(255, 255, 255, 255), 0, true), 54321), L4Proto::TCP, false);

  int64_t now = NowMicros();

  ConnectionTracker tracker;
  tracker.Update({conn1, conn2, conn3, conn4, conn5}, {}, now);

  auto state = tracker.FetchConnState(true);
  EXPECT_THAT(state, UnorderedElementsAre(
                         std::make_pair(conn13_normalized, ConnStatus(now, true)),
                         std::make_pair(conn24_normalized, ConnStatus(now, true))));

  auto state2 = tracker.FetchConnState(true);
  EXPECT_EQ(state, state2);

  UnorderedSet<Address> public_ips = {Address(35, 127, 0, 15)};
  tracker.UpdateKnownPublicIPs(std::move(public_ips));

  UnorderedMap<Address::Family, std::vector<IPNet>> known_networks = {{Address::Family::IPV4, {IPNet(Address(35, 127, 1, 15), 24)}}};
  tracker.UpdateKnownIPNetworks(std::move(known_networks));

  auto state3 = tracker.FetchConnState(true);

  Connection conn1_normalized("xyz", Endpoint(IPNet(), 9999), Endpoint(IPNet(Address(35, 127, 0, 15), 0, true), 0), L4Proto::TCP, true);
  Connection conn2_normalized("xyz", Endpoint(), Endpoint(IPNet(Address(35, 127, 0, 15), 0, true), 54321), L4Proto::TCP, false);
  Connection conn3_normalized("xyz", Endpoint(IPNet(), 9999), Endpoint(IPNet(Address(255, 255, 255, 255), 0, true), 0), L4Proto::TCP, true);
  Connection conn4_normalized("xyz", Endpoint(), Endpoint(IPNet(Address(255, 255, 255, 255), 0, true), 54321), L4Proto::TCP, false);
  Connection conn5_normalized("xyz", Endpoint(IPNet(), 9999), Endpoint(IPNet(Address(35, 127, 1, 0), 24, false), 0), L4Proto::TCP, true);

  EXPECT_THAT(state3, UnorderedElementsAre(
                          std::make_pair(conn1_normalized, ConnStatus(now, true)),
                          std::make_pair(conn2_normalized, ConnStatus(now, true)),
                          std::make_pair(conn3_normalized, ConnStatus(now, true)),
                          std::make_pair(conn4_normalized, ConnStatus(now, true)),
                          std::make_pair(conn5_normalized, ConnStatus(now, true))));
}

TEST(ConnTrackerTest, TestComputeDeltaEmptyOldState) {
  Endpoint a(Address(192, 168, 0, 1), 80);
  Endpoint b(Address(192, 168, 1, 10), 9999);

  Connection conn1("xyz", a, b, L4Proto::TCP, true);
  Connection conn2("xzy", b, a, L4Proto::TCP, false);

  int64_t now = 0;

  ConnMap orig_state = {{conn1, ConnStatus(now, true)},
                        {conn2, ConnStatus(now, true)}};
  ConnMap state1 = orig_state;
  ConnMap state2;

  // ComputeDelta on an empty old state should just copy over the entire state.
  CT::ComputeDelta(state1, &state2, now);
  EXPECT_EQ(state1, state2);
}

TEST(ConnTrackerTest, TestComputeDeltaSameState) {
  Endpoint a(Address(192, 168, 0, 1), 80);
  Endpoint b(Address(192, 168, 1, 10), 9999);

  Connection conn1("xyz", a, b, L4Proto::TCP, true);
  Connection conn2("xzy", b, a, L4Proto::TCP, false);

  int64_t now = 0;

  ConnMap orig_state = {{conn1, ConnStatus(now, true)},
                        {conn2, ConnStatus(now, true)}};
  ConnMap state1 = orig_state;
  ConnMap state2 = orig_state;

  // ComputeDelta on two equal states should result in an empty delta.
  CT::ComputeDelta(state1, &state2, now);
  EXPECT_THAT(state2, IsEmpty());
}

TEST(ConnTrackerTest, TestComputeDeltaRemoveConnection) {
  Endpoint a(Address(192, 168, 0, 1), 80);
  Endpoint b(Address(192, 168, 1, 10), 9999);

  Connection conn1("xyz", a, b, L4Proto::TCP, true);
  Connection conn2("xzy", b, a, L4Proto::TCP, false);

  int64_t now = 0;

  ConnMap state1 = {{conn2, ConnStatus(now, true)}};
  ConnMap state2 = {{conn1, ConnStatus(now, true)},
                    {conn2, ConnStatus(now, true)}};

  // Removing a connection from the active state should have it appear as inactive in the delta (with the previous
  // timestamp).
  CT::ComputeDelta(state1, &state2, now);
  EXPECT_THAT(state2, UnorderedElementsAre(std::make_pair(conn1, ConnStatus(now, false))));
}

TEST(ConnTrackerTest, TestComputeDeltaChangeTimeStamp) {
  Endpoint a(Address(192, 168, 0, 1), 80);
  Endpoint b(Address(192, 168, 1, 10), 9999);

  Connection conn1("xyz", a, b, L4Proto::TCP, true);
  Connection conn2("xzy", b, a, L4Proto::TCP, false);

  int64_t now = 0;
  int64_t now2 = 1000;

  ConnMap state1 = {{conn1, ConnStatus(now, true)},
                    {conn2, ConnStatus(now, true)}};
  ConnMap state2 = state1;

  state1[conn1] = ConnStatus(now2, true);

  // Just updating the timestamp of an active connection should not make it appear in the delta.
  CT::ComputeDelta(state1, &state2, now);
  EXPECT_THAT(state2, IsEmpty());
}

TEST(ConnTrackerTest, TestComputeDeltaSetToInactive) {
  Endpoint a(Address(192, 168, 0, 1), 80);
  Endpoint b(Address(192, 168, 1, 10), 9999);

  Connection conn1("xyz", a, b, L4Proto::TCP, true);
  Connection conn2("xzy", b, a, L4Proto::TCP, false);
  int64_t now = 0;
  int64_t now2 = 1000;
  int64_t now3 = 100000000; //100 seconds

  ConnMap state1 = {{conn1, ConnStatus(now, true)},
                    {conn2, ConnStatus(now, true)}};
  ConnMap state2 = state1;

  state1[conn1] = ConnStatus(now2, false);

  // Marking a connection as inactive should make it appear as inactive in the delta.
  CT::ComputeDelta(state1, &state2, now3);
  EXPECT_THAT(state2, UnorderedElementsAre(std::make_pair(conn1, ConnStatus(now2, false))));
}

TEST(ConnTrackerTest, TestComputeDeltaInactiveRemovedIsntInDelta) {
  // A connection that was already inactive no longer showing up at all in the new state should not appear in the delta.
  Endpoint a(Address(192, 168, 0, 1), 80);
  Endpoint b(Address(192, 168, 1, 10), 9999);

  Connection conn1("xyz", a, b, L4Proto::TCP, true);
  Connection conn2("xzy", b, a, L4Proto::TCP, false);
  int64_t now = 0;

  ConnMap state1 = {{conn1, ConnStatus(now, true)},
                    {conn2, ConnStatus(now, true)}};
  ConnMap state2 = state1;

  state2[conn1].SetActive(false);
  state1.erase(conn1);

  CT::ComputeDelta(state1, &state2, now);
  EXPECT_THAT(state2, IsEmpty());
}
/*
TEST(ConnTrackerTest, TestApplyAfterglow) {
  //Afterglow should not do anything as both connections are active
  Endpoint a(Address(192, 168, 0, 1), 80);
  Endpoint b(Address(192, 168, 1, 10), 9999);

  Connection conn1("xyz", a, b, L4Proto::TCP, true);
  Connection conn2("xzy", b, a, L4Proto::TCP, false);
  int64_t now = 0;
  int64_t afterglow_period = 1000;

  ConnMap original_state = {{conn1, ConnStatus(now, true)},
                            {conn2, ConnStatus(now, true)}};
  ConnMap state = original_state;

  CT::ApplyAfterglow(state, now, afterglow_period);
  EXPECT_THAT(state, original_state);
}

TEST(ConnTrackerTest, TestApplyAfterglowActivateBeforeAfterglowPeriod) {
  //Afterglow should flip the inactive connection to be active, since it was recently active
  Endpoint a(Address(192, 168, 0, 1), 80);
  Endpoint b(Address(192, 168, 1, 10), 9999);

  Connection conn1("xyz", a, b, L4Proto::TCP, true);
  Connection conn2("xzy", b, a, L4Proto::TCP, false);
  int64_t now = 0;
  int64_t now2 = 500;
  int64_t afterglow_period = 1000;

  ConnMap original_state = {{conn1, ConnStatus(now, true)},
                            {conn2, ConnStatus(now, false)}};
  ConnMap expected_state = {{conn1, ConnStatus(now, true)},
                            {conn2, ConnStatus(now, true)}};
  ConnMap state = original_state;

  CT::ApplyAfterglow(state, now2, afterglow_period);
  EXPECT_THAT(state, expected_state);
}

TEST(ConnTrackerTest, TestApplyAfterglowDontActivateAfterAfterglowPeriod) {
  //Afterglow should not flip the inactive connection to be active, because the afterglow period is expired
  Endpoint a(Address(192, 168, 0, 1), 80);
  Endpoint b(Address(192, 168, 1, 10), 9999);

  Connection conn1("xyz", a, b, L4Proto::TCP, true);
  Connection conn2("xzy", b, a, L4Proto::TCP, false);
  int64_t now = 0;
  int64_t now2 = 5000;
  int64_t afterglow_period = 1000;

  ConnMap original_state = {{conn1, ConnStatus(now, true)},
                            {conn2, ConnStatus(now, false)}};
  ConnMap state = original_state;

  CT::ApplyAfterglow(state, now2, afterglow_period);
  EXPECT_THAT(state, original_state);
}

TEST(ConnTrackerTest, TestComputeDeltaWithAfterglow) {
  //Afterglow flips the inactive connection to active, so it does not show up in delta
  Endpoint a(Address(192, 168, 0, 1), 80);
  Endpoint b(Address(192, 168, 1, 10), 9999);

  Connection conn1("xyz", a, b, L4Proto::TCP, true);
  Connection conn2("xzy", b, a, L4Proto::TCP, false);
  int64_t now = 0;

  ConnMap state1 = {{conn1, ConnStatus(now, true)},
                    {conn2, ConnStatus(now, false)}};
  ConnMap state2 = {{conn1, ConnStatus(now, true)},
                    {conn2, ConnStatus(now, true)}};

  CT::ComputeDeltaWithAfterglow(state1, &state2, now);
  EXPECT_THAT(state2, IsEmpty());
}

TEST(ConnTrackerTest, TestComputeDeltaWithAfterglowExpired) {
  //The inactive connection is outside of the afterglow period so it is not flipped to be in the active state
  //so it appears in delta
  Endpoint a(Address(192, 168, 0, 1), 80);
  Endpoint b(Address(192, 168, 1, 10), 9999);

  Connection conn1("xyz", a, b, L4Proto::TCP, true);
  Connection conn2("xzy", b, a, L4Proto::TCP, false);
  int64_t now = 0;
  int64_t now2 = 400000000;

  ConnMap state1 = {{conn1, ConnStatus(now, true)},
                    {conn2, ConnStatus(now, false)}};
  ConnMap state2 = {{conn1, ConnStatus(now, true)},
                    {conn2, ConnStatus(now, true)}};

  CT::ComputeDeltaWithAfterglow(state1, &state2, now2);
  EXPECT_THAT(state2, UnorderedElementsAre(std::make_pair(conn2, ConnStatus(now, false))));
}
*/

}  // namespace

}  // namespace collector
