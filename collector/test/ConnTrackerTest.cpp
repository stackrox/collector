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
* version. */

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

  int64_t now = 1000;

  ConnectionTracker tracker;
  tracker.AddConnection(conn1, now);
  tracker.AddConnection(conn2, now);

  auto state = tracker.FetchConnState();
  EXPECT_THAT(state, UnorderedElementsAre(std::make_pair(conn1, ConnStatus(now, true)), std::make_pair(conn2, ConnStatus(now, true))));

  auto state2 = tracker.FetchConnState();
  EXPECT_EQ(state, state2);

  int64_t now2 = 2000;
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

  int64_t now = 1000;

  ConnectionTracker tracker;
  tracker.Update({conn1, conn2}, {}, now);

  auto state = tracker.FetchConnState();
  EXPECT_THAT(state, UnorderedElementsAre(std::make_pair(conn1, ConnStatus(now, true)), std::make_pair(conn2, ConnStatus(now, true))));

  auto state2 = tracker.FetchConnState();
  EXPECT_EQ(state, state2);

  int64_t now2 = 1005;
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
  int64_t now = 1000;
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

  int64_t now = 1000;

  ConnectionTracker tracker;
  tracker.Update({conn1, conn2, conn3, conn4}, {}, now);

  auto state = tracker.FetchConnState(true);
  EXPECT_THAT(state, UnorderedElementsAre(
                         std::make_pair(conn13_normalized, ConnStatus(now, true)),
                         std::make_pair(conn24_normalized, ConnStatus(now, true))));

  auto state2 = tracker.FetchConnState(true);
  EXPECT_EQ(state, state2);

  int64_t now2 = 1005;
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

  int64_t now3 = 1010;
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

  int64_t now = 1000;

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

/*
 * Start block of tests with at most one connection in old_state and at most ond connection in new_state
 * The names of the tests specify what the state of the old connection is and what the state of the new connection is
 * The format of the test names is as follows
 *
 * TestComputeDeltaAfterglow{old_state}Old{new_state}New
 *
 * The possible states are "Active", "InactiveUnexpired", "InactiveExpired", and "No"
 *
 * "Active" means that IsActive() returns true
 * "InactiveUnexpired" means that IsActive() returns false, but WasRecentlyActive returns true, i.e the connection was closed within the afterglow period
 * "InactiveExpired" means IsActive() returns false and WasRecentlyActive returns false, i.e the connection was closed outside of the afterglow period
 * "No" means that there is no connection
 *
 * There is also a test with the name TestComputeDeltaAfterglowInactiveExpiredOldInactiveExpiredNewDifferentTimeStamp. This specifies that WasRecentlyActive()
 * returns false for both new and old connections and that they have different LastActiveTime().
 *
 * There is also a test with the name TestComputeDeltaAfterglowInactiveExpiredOldInactiveExpiredNewSameTimeStamp. This specifies that WasRecentlyActive()
 * returns false for both new and old connections and that they have the same LastActiveTime().
 *
 * Another exception is TestComputeDeltaAfterglowInactiveUnexpiredWithinAfterglowOldNoNew. For the old connection IsActive() returns false, WasRecentlyActive()
 * returns true, and IsInAfterglowPeriod(conn.second, now, afterglow_period_micros) returns false
 */
//
TEST(ConnTrackerTest, TestComputeDeltaAfterglowActiveOldActiveNew) {
  // If both old and new connections are active delta should be empty
  Endpoint a(Address(192, 168, 0, 1), 80);
  Endpoint b(Address(192, 168, 1, 10), 9999);

  Connection conn1("xyz", a, b, L4Proto::TCP, true);
  int64_t connection_time1 = 990;
  int64_t connection_time2 = 1990;
  int64_t now = 2000;
  int64_t time_at_last_scrape = 1000;
  int64_t afterglow_period_micros = 50;

  ConnMap old_state = {{conn1, ConnStatus(connection_time1, true)}};
  ConnMap new_state = {{conn1, ConnStatus(connection_time2, true)}};
  ConnMap delta;
  CT::ComputeDeltaAfterglow(new_state, old_state, delta, now, time_at_last_scrape, afterglow_period_micros);
  EXPECT_THAT(delta, IsEmpty());
}

TEST(ConnTrackerTest, TestComputeDeltaAfterglowActiveOldInactiveUnexpired) {
  Endpoint a(Address(192, 168, 0, 1), 80);
  Endpoint b(Address(192, 168, 1, 10), 9999);

  Connection conn1("xyz", a, b, L4Proto::TCP, true);
  int64_t connection_time1 = 990;
  int64_t connection_time2 = 1990;
  int64_t now = 2000;
  int64_t time_at_last_scrape = 1000;
  int64_t afterglow_period_micros = 50;

  ConnMap old_state = {{conn1, ConnStatus(connection_time1, true)}};
  ConnMap new_state = {{conn1, ConnStatus(connection_time2, false)}};
  ConnMap delta;
  CT::ComputeDeltaAfterglow(new_state, old_state, delta, now, time_at_last_scrape, afterglow_period_micros);
  EXPECT_THAT(delta, IsEmpty());
}

TEST(ConnTrackerTest, TestComputeDeltaAfterglowInactiveUnexpiredOldActiveNew) {
  Endpoint a(Address(192, 168, 0, 1), 80);
  Endpoint b(Address(192, 168, 1, 10), 9999);

  Connection conn1("xyz", a, b, L4Proto::TCP, true);
  int64_t connection_time1 = 990;
  int64_t connection_time2 = 1990;
  int64_t now = 2000;
  int64_t time_at_last_scrape = 1000;
  int64_t afterglow_period_micros = 50;

  ConnMap old_state = {{conn1, ConnStatus(connection_time1, false)}};
  ConnMap new_state = {{conn1, ConnStatus(connection_time2, true)}};
  ConnMap delta;
  CT::ComputeDeltaAfterglow(new_state, old_state, delta, now, time_at_last_scrape, afterglow_period_micros);
  EXPECT_THAT(delta, IsEmpty());
}

TEST(ConnTrackerTest, TestComputeDeltaAfterglowInactiveUnexpiredOldInactiveUnexpiredNew) {
  Endpoint a(Address(192, 168, 0, 1), 80);
  Endpoint b(Address(192, 168, 1, 10), 9999);

  Connection conn1("xyz", a, b, L4Proto::TCP, true);
  int64_t connection_time1 = 990;
  int64_t connection_time2 = 1990;
  int64_t now = 2000;
  int64_t time_at_last_scrape = 1000;
  int64_t afterglow_period_micros = 50;

  ConnMap old_state = {{conn1, ConnStatus(connection_time1, false)}};
  ConnMap new_state = {{conn1, ConnStatus(connection_time2, false)}};
  ConnMap delta;
  CT::ComputeDeltaAfterglow(new_state, old_state, delta, now, time_at_last_scrape, afterglow_period_micros);
  EXPECT_THAT(delta, IsEmpty());
}

TEST(ConnTrackerTest, TestComputeDeltaAfterglowActiveOldInactiveExpiredNew) {
  Endpoint a(Address(192, 168, 0, 1), 80);
  Endpoint b(Address(192, 168, 1, 10), 9999);

  Connection conn1("xyz", a, b, L4Proto::TCP, true);
  int64_t connection_time1 = 990;
  int64_t connection_time2 = 1900;
  int64_t now = 2000;
  int64_t time_at_last_scrape = 1000;
  int64_t afterglow_period_micros = 50;

  ConnMap old_state = {{conn1, ConnStatus(connection_time1, true)}};
  ConnMap new_state = {{conn1, ConnStatus(connection_time2, false)}};
  ConnMap delta;
  CT::ComputeDeltaAfterglow(new_state, old_state, delta, now, time_at_last_scrape, afterglow_period_micros);
  EXPECT_THAT(delta, new_state);
}

TEST(ConnTrackerTest, TestComputeDeltaAfterglowInactiveUnexpiredOldInactiveExpiredNew) {
  Endpoint a(Address(192, 168, 0, 1), 80);
  Endpoint b(Address(192, 168, 1, 10), 9999);

  Connection conn1("xyz", a, b, L4Proto::TCP, true);
  int64_t connection_time1 = 990;
  int64_t connection_time2 = 1900;
  int64_t now = 2000;
  int64_t time_at_last_scrape = 1000;
  int64_t afterglow_period_micros = 50;

  ConnMap old_state = {{conn1, ConnStatus(connection_time1, false)}};
  ConnMap new_state = {{conn1, ConnStatus(connection_time2, false)}};
  ConnMap delta;
  CT::ComputeDeltaAfterglow(new_state, old_state, delta, now, time_at_last_scrape, afterglow_period_micros);
  EXPECT_THAT(delta, new_state);
}

TEST(ConnTrackerTest, TestComputeDeltaAfterglowInactiveExpiredOldActiveNew) {
  Endpoint a(Address(192, 168, 0, 1), 80);
  Endpoint b(Address(192, 168, 1, 10), 9999);

  Connection conn1("xyz", a, b, L4Proto::TCP, true);
  int64_t connection_time1 = 900;
  int64_t connection_time2 = 1900;
  int64_t now = 2000;
  int64_t time_at_last_scrape = 1000;
  int64_t afterglow_period_micros = 50;

  ConnMap old_state = {{conn1, ConnStatus(connection_time1, false)}};
  ConnMap new_state = {{conn1, ConnStatus(connection_time2, true)}};
  ConnMap delta;
  CT::ComputeDeltaAfterglow(new_state, old_state, delta, now, time_at_last_scrape, afterglow_period_micros);
  EXPECT_THAT(delta, new_state);
}

TEST(ConnTrackerTest, TestComputeDeltaAfterglowInactiveExpiredOldInactiveUnexpiredNew) {
  Endpoint a(Address(192, 168, 0, 1), 80);
  Endpoint b(Address(192, 168, 1, 10), 9999);

  Connection conn1("xyz", a, b, L4Proto::TCP, true);
  int64_t connection_time1 = 900;
  int64_t connection_time2 = 1990;
  int64_t now = 2000;
  int64_t time_at_last_scrape = 1000;
  int64_t afterglow_period_micros = 50;

  ConnMap old_state = {{conn1, ConnStatus(connection_time1, false)}};
  ConnMap new_state = {{conn1, ConnStatus(connection_time2, false)}};
  ConnMap delta;
  CT::ComputeDeltaAfterglow(new_state, old_state, delta, now, time_at_last_scrape, afterglow_period_micros);
  EXPECT_THAT(delta, new_state);
}

TEST(ConnTrackerTest, TestComputeDeltaAfterglowInactiveExpiredOldInactiveExpiredNewDifferentTimeStamp) {
  Endpoint a(Address(192, 168, 0, 1), 80);
  Endpoint b(Address(192, 168, 1, 10), 9999);

  Connection conn1("xyz", a, b, L4Proto::TCP, true);
  int64_t connection_time1 = 900;
  int64_t connection_time2 = 1900;
  int64_t now = 2000;
  int64_t time_at_last_scrape = 1000;
  int64_t afterglow_period_micros = 50;

  ConnMap old_state = {{conn1, ConnStatus(connection_time1, false)}};
  ConnMap new_state = {{conn1, ConnStatus(connection_time2, false)}};
  ConnMap delta;
  CT::ComputeDeltaAfterglow(new_state, old_state, delta, now, time_at_last_scrape, afterglow_period_micros);
  EXPECT_THAT(delta, new_state);
}

TEST(ConnTrackerTest, TestComputeDeltaAfterglowInactiveExpiredOldInactiveExpiredNewSameTimeStamp) {
  // This one is probably not possible in real life
  Endpoint a(Address(192, 168, 0, 1), 80);
  Endpoint b(Address(192, 168, 1, 10), 9999);

  Connection conn1("xyz", a, b, L4Proto::TCP, true);
  int64_t connection_time1 = 900;
  int64_t connection_time2 = 900;
  int64_t now = 2000;
  int64_t time_at_last_scrape = 2000;
  int64_t afterglow_period_micros = 50;

  ConnMap old_state = {{conn1, ConnStatus(connection_time1, false)}};
  ConnMap new_state = {{conn1, ConnStatus(connection_time2, false)}};
  ConnMap delta;
  CT::ComputeDeltaAfterglow(new_state, old_state, delta, now, time_at_last_scrape, afterglow_period_micros);
  EXPECT_THAT(delta, IsEmpty());
}

TEST(ConnTrackerTest, TestComputeDeltaAfterglowActiveOldNoNew) {
  Endpoint a(Address(192, 168, 0, 1), 80);
  Endpoint b(Address(192, 168, 1, 10), 9999);

  Connection conn1("xyz", a, b, L4Proto::TCP, true);
  int64_t connection_time1 = 990;
  int64_t now = 2000;
  int64_t time_at_last_scrape = 1000;
  int64_t afterglow_period_micros = 50;

  ConnMap old_state = {{conn1, ConnStatus(connection_time1, true)}};
  ConnMap new_state;
  ConnMap delta;
  ConnMap expected_delta = {{conn1, ConnStatus(connection_time1, false)}};
  CT::ComputeDeltaAfterglow(new_state, old_state, delta, now, time_at_last_scrape, afterglow_period_micros);
  EXPECT_THAT(delta, expected_delta);
}

TEST(ConnTrackerTest, TestComputeDeltaAfterglowInactiveUnexpiredNoNew) {
  Endpoint a(Address(192, 168, 0, 1), 80);
  Endpoint b(Address(192, 168, 1, 10), 9999);

  Connection conn1("xyz", a, b, L4Proto::TCP, true);
  int64_t connection_time1 = 990;
  int64_t now = 2000;
  int64_t time_at_last_scrape = 1000;
  int64_t afterglow_period_micros = 50;

  ConnMap old_state = {{conn1, ConnStatus(connection_time1, false)}};
  ConnMap new_state;
  ConnMap delta;
  CT::ComputeDeltaAfterglow(new_state, old_state, delta, now, time_at_last_scrape, afterglow_period_micros);
  EXPECT_THAT(delta, old_state);
}

TEST(ConnTrackerTest, TestComputeDeltaAfterglowInactiveUnexpiredWithinAfterglowNoNew) {
  Endpoint a(Address(192, 168, 0, 1), 80);
  Endpoint b(Address(192, 168, 1, 10), 9999);

  Connection conn1("xyz", a, b, L4Proto::TCP, true);
  int64_t connection_time1 = 990;
  int64_t now = 2000;
  int64_t time_at_last_scrape = 1000;
  int64_t afterglow_period_micros = 5000;

  ConnMap old_state = {{conn1, ConnStatus(connection_time1, false)}};
  ConnMap new_state;
  ConnMap delta;
  CT::ComputeDeltaAfterglow(new_state, old_state, delta, now, time_at_last_scrape, afterglow_period_micros);
  EXPECT_THAT(delta, IsEmpty());
}

TEST(ConnTrackerTest, TestComputeDeltaAfterglowInactiveExpiredNoNew) {
  Endpoint a(Address(192, 168, 0, 1), 80);
  Endpoint b(Address(192, 168, 1, 10), 9999);

  Connection conn1("xyz", a, b, L4Proto::TCP, true);
  int64_t connection_time1 = 900;
  int64_t now = 2000;
  int64_t time_at_last_scrape = 1000;
  int64_t afterglow_period_micros = 50;

  ConnMap old_state = {{conn1, ConnStatus(connection_time1, false)}};
  ConnMap new_state;
  ConnMap delta;
  CT::ComputeDeltaAfterglow(new_state, old_state, delta, now, time_at_last_scrape, afterglow_period_micros);
  EXPECT_THAT(delta, IsEmpty());
}

TEST(ConnTrackerTest, TestComputeDeltaAfterglowNoOldActiveNew) {
  Endpoint a(Address(192, 168, 0, 1), 80);
  Endpoint b(Address(192, 168, 1, 10), 9999);

  Connection conn1("xyz", a, b, L4Proto::TCP, true);
  int64_t connection_time1 = 900;
  int64_t now = 2000;
  int64_t time_at_last_scrape = 1000;
  int64_t afterglow_period_micros = 50;

  ConnMap old_state;
  ConnMap new_state = {{conn1, ConnStatus(connection_time1, true)}};
  ConnMap delta;
  CT::ComputeDeltaAfterglow(new_state, old_state, delta, now, time_at_last_scrape, afterglow_period_micros);
  EXPECT_THAT(delta, new_state);
}

TEST(ConnTrackerTest, TestComputeDeltaAfterglowNoOldInactiveUnexpiredNew) {
  Endpoint a(Address(192, 168, 0, 1), 80);
  Endpoint b(Address(192, 168, 1, 10), 9999);

  Connection conn1("xyz", a, b, L4Proto::TCP, true);
  int64_t connection_time1 = 1990;
  int64_t now = 2000;
  int64_t time_at_last_scrape = 1000;
  int64_t afterglow_period_micros = 50;

  ConnMap old_state;
  ConnMap new_state = {{conn1, ConnStatus(connection_time1, false)}};
  ConnMap delta;
  CT::ComputeDeltaAfterglow(new_state, old_state, delta, now, time_at_last_scrape, afterglow_period_micros);
  EXPECT_THAT(delta, new_state);
}

TEST(ConnTrackerTest, TestComputeDeltaAfterglowNoOldInactiveExpiredNew) {
  Endpoint a(Address(192, 168, 0, 1), 80);
  Endpoint b(Address(192, 168, 1, 10), 9999);

  Connection conn1("xyz", a, b, L4Proto::TCP, true);
  int64_t connection_time1 = 1900;
  int64_t now = 2000;
  int64_t time_at_last_scrape = 1000;
  int64_t afterglow_period_micros = 50;

  ConnMap old_state;
  ConnMap new_state = {{conn1, ConnStatus(connection_time1, false)}};
  ConnMap delta;
  CT::ComputeDeltaAfterglow(new_state, old_state, delta, now, time_at_last_scrape, afterglow_period_micros);
  EXPECT_THAT(delta, new_state);
}

TEST(ConnTrackerTest, TestComputeDeltaAfterglowEmptyOldState) {
  Endpoint a(Address(192, 168, 0, 1), 80);
  Endpoint b(Address(192, 168, 1, 10), 9999);

  Connection conn1("xyz", a, b, L4Proto::TCP, true);
  Connection conn2("xzy", b, a, L4Proto::TCP, false);

  int64_t now = 1000;
  int64_t time_at_last_scrape = 500;
  int64_t connection_time = 750;
  int64_t afterglow_period_micros = 20000000;  // 20 seconds in microseconds

  ConnMap new_state = {{conn1, ConnStatus(connection_time, true)},
                       {conn2, ConnStatus(connection_time, true)}};
  ConnMap old_state, delta;

  // ComputeDeltaAfterglow on an empty old state should just copy over the entire state.
  CT::ComputeDeltaAfterglow(new_state, old_state, delta, now, time_at_last_scrape, afterglow_period_micros);
  EXPECT_EQ(new_state, delta);
}

/*
* End block of tests with at most one connection in old_state and at most ond connection in new_state
*/

TEST(ConnTrackerTest, TestComputeDeltaAfterglowSameState) {
  Endpoint a(Address(192, 168, 0, 1), 80);
  Endpoint b(Address(192, 168, 1, 10), 9999);

  Connection conn1("xyz", a, b, L4Proto::TCP, true);
  Connection conn2("xzy", b, a, L4Proto::TCP, false);

  int64_t now = 1000;
  int64_t time_at_last_scrape = 500;
  int64_t connection_time = 750;
  int64_t afterglow_period_micros = 20000000;  // 20 seconds in microseconds

  ConnMap new_state = {{conn1, ConnStatus(connection_time, true)},
                       {conn2, ConnStatus(connection_time, true)}};
  ConnMap old_state = new_state;
  ConnMap delta;

  // ComputeDeltaAfterglow on two equal states should result in an empty delta.
  CT::ComputeDeltaAfterglow(new_state, old_state, delta, now, time_at_last_scrape, afterglow_period_micros);
  EXPECT_THAT(delta, IsEmpty());
}

TEST(ConnTrackerTest, TestComputeDeltaAfterglowRemoveConnectionExpiredAfterglow) {
  Endpoint a(Address(192, 168, 0, 1), 80);
  Endpoint b(Address(192, 168, 1, 10), 9999);

  Connection conn1("xyz", a, b, L4Proto::TCP, true);
  Connection conn2("xzy", b, a, L4Proto::TCP, false);

  int64_t now = 1000;
  int64_t time_at_last_scrape = 500;
  int64_t connection_time1 = 490;
  int64_t connection_time2 = 990;
  int64_t afterglow_period_micros = 20;

  ConnMap new_state = {{conn2, ConnStatus(connection_time2, true)}};
  ConnMap old_state = {{conn1, ConnStatus(connection_time1, true)},
                       {conn2, ConnStatus(connection_time1, true)}};
  ConnMap delta;

  // Removing a connection from the active state should have it appear as inactive in the delta (with the previous
  // timestamp), if the old connection is not in the afterglow period.
  CT::ComputeDeltaAfterglow(new_state, old_state, delta, now, time_at_last_scrape, afterglow_period_micros);
  EXPECT_THAT(delta, UnorderedElementsAre(std::make_pair(conn1, ConnStatus(connection_time1, false))));
}

TEST(ConnTrackerTest, TestComputeDeltaAfterglowRemoveConnection) {
  Endpoint a(Address(192, 168, 0, 1), 80);
  Endpoint b(Address(192, 168, 1, 10), 9999);

  Connection conn1("xyz", a, b, L4Proto::TCP, true);
  Connection conn2("xzy", b, a, L4Proto::TCP, false);

  int64_t now = 1000;
  int64_t time_at_last_scrape = 500;
  int64_t connection_time1 = 490;
  int64_t connection_time2 = 990;
  int64_t afterglow_period_micros = 20000000;  // 20 seconds in microseconds

  ConnMap new_state = {{conn2, ConnStatus(connection_time2, true)}};
  ConnMap old_state = {{conn1, ConnStatus(connection_time1, true)},
                       {conn2, ConnStatus(connection_time1, true)}};
  ConnMap delta;

  // Removing a connection from the active state should not have it appear in the delta if the afterglow period has not
  // passed for the old state
  CT::ComputeDeltaAfterglow(new_state, old_state, delta, now, time_at_last_scrape, afterglow_period_micros);
  EXPECT_THAT(delta, IsEmpty());
}

TEST(ConnTrackerTest, TestComputeDeltaAfterglowChangeTimeStamp) {
  Endpoint a(Address(192, 168, 0, 1), 80);
  Endpoint b(Address(192, 168, 1, 10), 9999);

  Connection conn1("xyz", a, b, L4Proto::TCP, true);
  Connection conn2("xzy", b, a, L4Proto::TCP, false);

  int64_t now = 2000;
  int64_t time_at_last_scrape = 1000;
  int64_t connection_time1 = 990;
  int64_t connection_time2 = 1990;

  int64_t afterglow_period_micros = 20000000;  // 20 seconds in microseconds

  ConnMap new_state = {{conn1, ConnStatus(connection_time2, true)},
                       {conn2, ConnStatus(connection_time2, true)}};
  ConnMap old_state = new_state;
  ConnMap delta;

  new_state[conn1] = ConnStatus(connection_time1, true);

  // Just updating the timestamp of an active connection should not make it appear in the delta.
  CT::ComputeDeltaAfterglow(new_state, old_state, delta, now, time_at_last_scrape, afterglow_period_micros);
  EXPECT_THAT(delta, IsEmpty());
}

TEST(ConnTrackerTest, TestComputeDeltaAfterglowSetToInactive) {
  Endpoint a(Address(192, 168, 0, 1), 80);
  Endpoint b(Address(192, 168, 1, 10), 9999);

  Connection conn1("xyz", a, b, L4Proto::TCP, true);
  Connection conn2("xzy", b, a, L4Proto::TCP, false);
  int64_t now = 100000000;  //100 seconds
  int64_t time_at_last_scrape = 2000;
  int64_t connection_time1 = 1000;
  int64_t connection_time2 = 3000;
  int64_t afterglow_period_micros = 20000000;  // 20 seconds in microseconds

  ConnMap old_state = {{conn1, ConnStatus(connection_time1, true)},
                       {conn2, ConnStatus(connection_time1, true)}};
  ConnMap new_state = {{conn1, ConnStatus(connection_time2, false)},
                       {conn2, ConnStatus(connection_time2, false)}};
  ConnMap delta;

  // Marking a connection as inactive should make it appear as inactive in the delta if the connection has expired in the old_state
  CT::ComputeDeltaAfterglow(new_state, old_state, delta, now, time_at_last_scrape, afterglow_period_micros);
  EXPECT_THAT(delta, new_state);
}

TEST(ConnTrackerTest, TestComputeDeltaAfterglowOldAndNewAreInactive) {
  // If both old and new connections are inactive and there is a timestamp change the new connection should be in the delta
  Endpoint a(Address(192, 168, 0, 1), 80);
  Endpoint b(Address(192, 168, 1, 10), 9999);

  Connection conn1("xyz", a, b, L4Proto::TCP, true);
  Connection conn2("xzy", b, a, L4Proto::TCP, false);
  int64_t connection_time1 = 1000;
  int64_t connection_time2 = 3000;
  int64_t now = 2000;
  int64_t time_at_last_scrape = 4000;
  int64_t afterglow_period_micros = 50;

  ConnMap old_state = {{conn1, ConnStatus(connection_time1, false)}};
  ConnMap new_state = {{conn1, ConnStatus(connection_time2, false)}};
  ConnMap delta;
  CT::ComputeDeltaAfterglow(new_state, old_state, delta, now, time_at_last_scrape, afterglow_period_micros);
  EXPECT_THAT(delta, new_state);
}

TEST(ConnTrackerTest, TestComputeDeltaAfterglowInactiveRemovedIsntInDelta) {
  // A connection that was already inactive no longer showing up at all in the new state should not appear in the delta.
  Endpoint a(Address(192, 168, 0, 1), 80);
  Endpoint b(Address(192, 168, 1, 10), 9999);

  Connection conn1("xyz", a, b, L4Proto::TCP, true);
  Connection conn2("xzy", b, a, L4Proto::TCP, false);
  int64_t now = 2000;
  int64_t time_at_last_scrape = 1000;
  int64_t connection_time1 = 990;
  int64_t connection_time2 = 1990;
  int64_t afterglow_period_micros = 20000000;  // 20 seconds in microseconds

  ConnMap new_state = {{conn2, ConnStatus(connection_time2, true)}};
  ConnMap old_state = {{conn1, ConnStatus(connection_time1, false)},
                       {conn2, ConnStatus(connection_time1, true)}};
  ConnMap delta;
  CT::ComputeDeltaAfterglow(new_state, old_state, delta, now, time_at_last_scrape, afterglow_period_micros);
  EXPECT_THAT(delta, IsEmpty());
}

TEST(ConnTrackerTest, TestComputeDeltaAfterglowWithAfterglow) {
  //Afterglow flips the inactive connection to active, so it does not show up in delta
  Endpoint a(Address(192, 168, 0, 1), 80);
  Endpoint b(Address(192, 168, 1, 10), 9999);

  Connection conn1("xyz", a, b, L4Proto::TCP, true);
  Connection conn2("xzy", b, a, L4Proto::TCP, false);
  int64_t now = 1000;
  int64_t time_at_last_scrape = 3;
  int64_t connection_time1 = 2;
  int64_t connection_time2 = 999;
  int64_t afterglow_period_micros = 20000000;  // 20 seconds in microseconds

  ConnMap new_state = {{conn1, ConnStatus(connection_time2, true)},
                       {conn2, ConnStatus(connection_time2, false)}};
  ConnMap old_state = {{conn1, ConnStatus(connection_time1, true)},
                       {conn2, ConnStatus(connection_time1, true)}};
  ConnMap delta;

  CT::ComputeDeltaAfterglow(new_state, old_state, delta, now, time_at_last_scrape, afterglow_period_micros);
  EXPECT_THAT(delta, IsEmpty());
}

TEST(ConnTrackerTest, TestComputeDeltaAfterglowWithZeroAfterglowPeriod) {
  Endpoint a(Address(192, 168, 0, 1), 80);
  Endpoint b(Address(192, 168, 1, 10), 9999);

  Connection conn1("xyz", a, b, L4Proto::TCP, true);
  Connection conn2("xzy", b, a, L4Proto::TCP, false);
  int64_t now = 1000;
  int64_t time_at_last_scrape = 3;
  int64_t connection_time1 = time_at_last_scrape;
  int64_t connection_time2 = now;
  int64_t afterglow_period_micros = 0;

  ConnMap new_state = {{conn1, ConnStatus(connection_time2, true)},
                       {conn2, ConnStatus(connection_time2, false)}};
  ConnMap old_state = {{conn1, ConnStatus(connection_time1, true)},
                       {conn2, ConnStatus(connection_time1, true)}};
  ConnMap delta;

  CT::ComputeDeltaAfterglow(new_state, old_state, delta, now, time_at_last_scrape, afterglow_period_micros);
  EXPECT_THAT(delta, UnorderedElementsAre(std::make_pair(conn2, ConnStatus(now, false))));
}

TEST(ConnTrackerTest, TestComputeDeltaAfterglowWithZeroAfterglowPeriodEmptyNewState) {
  Endpoint a(Address(192, 168, 0, 1), 80);
  Endpoint b(Address(192, 168, 1, 10), 9999);

  Connection conn1("xyz", a, b, L4Proto::TCP, true);
  Connection conn2("xzy", b, a, L4Proto::TCP, false);
  int64_t now = 1000;
  int64_t time_at_last_scrape = 3;
  int64_t connection_time = 2;
  int64_t afterglow_period_micros = 0;

  ConnMap old_state = {{conn1, ConnStatus(connection_time, true)},
                       {conn2, ConnStatus(connection_time, true)}};
  ConnMap new_state;
  ConnMap delta;
  ConnMap expected_delta = {{conn1, ConnStatus(connection_time, false)},
                            {conn2, ConnStatus(connection_time, false)}};

  CT::ComputeDeltaAfterglow(new_state, old_state, delta, now, time_at_last_scrape, afterglow_period_micros);
  EXPECT_THAT(delta, expected_delta);
}
TEST(ConnTrackerTest, TestComputeDeltaAfterglowWithZeroAfterglowPeriodEmptyOldState) {
  Endpoint a(Address(192, 168, 0, 1), 80);
  Endpoint b(Address(192, 168, 1, 10), 9999);

  Connection conn1("xyz", a, b, L4Proto::TCP, true);
  Connection conn2("xzy", b, a, L4Proto::TCP, false);
  int64_t now = 1000;
  int64_t time_at_last_scrape = 3;
  int64_t connection_time = 999;
  int64_t afterglow_period_micros = 0;

  ConnMap new_state = {{conn1, ConnStatus(connection_time, true)},
                       {conn2, ConnStatus(connection_time, true)}};
  ConnMap old_state;
  ConnMap delta;

  CT::ComputeDeltaAfterglow(new_state, old_state, delta, now, time_at_last_scrape, afterglow_period_micros);
  EXPECT_THAT(delta, new_state);
}

TEST(ConnTrackerTest, TestComputeDeltaAfterglowWithZeroAfterglowPeriodNoChange) {
  Endpoint a(Address(192, 168, 0, 1), 80);
  Endpoint b(Address(192, 168, 1, 10), 9999);

  Connection conn1("xyz", a, b, L4Proto::TCP, true);
  Connection conn2("xzy", b, a, L4Proto::TCP, false);
  int64_t now = 1000;
  int64_t time_at_last_scrape = 3;
  int64_t connection_time1 = time_at_last_scrape;
  int64_t connection_time2 = now;
  int64_t afterglow_period_micros = 0;

  ConnMap new_state = {{conn1, ConnStatus(connection_time2, true)},
                       {conn2, ConnStatus(connection_time2, true)}};
  ConnMap old_state = {{conn1, ConnStatus(connection_time1, true)},
                       {conn2, ConnStatus(connection_time1, true)}};
  ConnMap delta;

  CT::ComputeDeltaAfterglow(new_state, old_state, delta, now, time_at_last_scrape, afterglow_period_micros);
  EXPECT_THAT(delta, IsEmpty());
}

TEST(ConnTrackerTest, TestComputeDeltaAfterglowWithAfterglowExpired) {
  //The inactive connection is outside of the afterglow period so it is not flipped to be in the active state
  //so it appears in delta
  Endpoint a(Address(192, 168, 0, 1), 80);
  Endpoint b(Address(192, 168, 1, 10), 9999);

  Connection conn1("xyz", a, b, L4Proto::TCP, true);
  Connection conn2("xzy", b, a, L4Proto::TCP, false);
  int64_t now = 400000000;
  int64_t time_at_last_scrape = 1000;
  int64_t connection_time1 = 1990;
  int64_t connection_time2 = 4000;
  int64_t afterglow_period_micros = 20000000;  // 20 seconds in microseconds

  ConnMap new_state = {{conn1, ConnStatus(connection_time2, true)},
                       {conn2, ConnStatus(connection_time2, false)}};
  ConnMap old_state = {{conn1, ConnStatus(connection_time1, true)},
                       {conn2, ConnStatus(connection_time1, true)}};
  ConnMap delta;

  CT::ComputeDeltaAfterglow(new_state, old_state, delta, now, time_at_last_scrape, afterglow_period_micros);
  EXPECT_THAT(delta, UnorderedElementsAre(std::make_pair(conn2, ConnStatus(connection_time2, false))));
}

TEST(ConnTrackerTest, TestComputeDeltaAfterglowWithAfterglowPeriodShorterThanScrapingInterval) {
  Endpoint a(Address(192, 168, 0, 1), 80);
  Endpoint b(Address(192, 168, 1, 10), 9999);

  Connection conn1("xyz", a, b, L4Proto::TCP, true);
  int64_t now = 20;
  int64_t time_at_last_scrape = 10;
  int64_t connection_time1 = 7;
  int64_t connection_time2 = 17;
  int64_t afterglow_period_micros = 5;

  ConnMap old_state = {{conn1, ConnStatus(connection_time1, true)}};
  ConnMap new_state = {{conn1, ConnStatus(connection_time2, true)}};
  ConnMap delta;

  CT::ComputeDeltaAfterglow(new_state, old_state, delta, now, time_at_last_scrape, afterglow_period_micros);
  EXPECT_THAT(delta, IsEmpty());
}

void GetNextAddress(int address_parts[4], int& port) {
  for (int i = 0; i < 4; i++) {
    address_parts[i]++;
    if (address_parts[i] < 256)
      break;
    else {
      address_parts[i] = 0;
    }
  }
  if (address_parts[3] == 256) {
    port++;
    for (int i = 0; i < 4; i++) address_parts[i] = 0;
  }
}

void CreateFakeState(ConnMap& afterglow_state, int num_endpoints, int num_connections, int64_t now) {
  int address_parts[4] = {0, 0, 0, 0};
  int port = 0;
  vector<Endpoint> endpoints(num_endpoints);
  vector<Connection> connections(num_connections * 2);

  for (int i = 0; i < num_endpoints; i++) {
    Address address(address_parts[0], address_parts[1], address_parts[2], address_parts[3]);
    Endpoint tempEndpoint(address, port);
    endpoints[i] = tempEndpoint;
    GetNextAddress(address_parts, port);
  }
  int connection_idx = 0;
  for (int i = 0; i < num_endpoints; i++) {
    for (int j = 0; j < num_endpoints; j++) {
      Connection tempConn1(std::to_string(connection_idx), endpoints[i], endpoints[j], L4Proto::TCP, false);
      Connection tempConn2(std::to_string(connection_idx), endpoints[j], endpoints[i], L4Proto::TCP, false);
      afterglow_state.insert({tempConn1, ConnStatus(now, false)});
      afterglow_state.insert({tempConn2, ConnStatus(now, false)});
      connection_idx += 2;
      if (connection_idx > num_connections) break;
    }
    if (connection_idx > num_connections) break;
  }
}

TEST(ConnTrackerTest, TestUpdateOldStateBenchmark) {
  int num_endpoints = 500;
  int num_connections = 20000;
  int64_t now1 = 1000;
  int64_t now2 = 2000;
  int64_t afterglow_period_micros = 20000000;  // 20 seconds in microseconds
  ConnMap old_state, new_state;

  CreateFakeState(old_state, num_endpoints, num_connections, now1);
  auto t1 = std::chrono::steady_clock::now();
  CT::UpdateOldState(&old_state, new_state, now2, afterglow_period_micros);
  auto t2 = std::chrono::steady_clock::now();
  std::chrono::duration<double, std::milli> dur = t2 - t1;
  std::cout << "old_state.size()= " << old_state.size() << std::endl;
  std::cout << "new_state.size()= " << new_state.size() << std::endl;
  std::cout << "Time taken by UpdateOldState= " << dur.count() << " ms\n";
}

TEST(ConnTrackerTest, TestUpdateOldStateBenchmark2) {
  int num_endpoints = 500;
  int num_connections = 20000;
  int64_t now1 = 1000;
  int64_t now2 = 2000;
  int64_t afterglow_period_micros = 20000000;  // 20 seconds in microseconds
  ConnMap old_state, new_state;

  CreateFakeState(new_state, num_endpoints, num_connections, now1);
  auto t1 = std::chrono::steady_clock::now();
  CT::UpdateOldState(&old_state, new_state, now2, afterglow_period_micros);
  auto t2 = std::chrono::steady_clock::now();
  std::chrono::duration<double, std::milli> dur = t2 - t1;
  std::cout << "old_state.size()= " << old_state.size() << std::endl;
  std::cout << "new_state.size()= " << new_state.size() << std::endl;
  std::cout << "Time taken by UpdateOldState= " << dur.count() << " ms\n";
}

TEST(ConnTrackerTest, TestComputeDeltaAfterglowBenchmark) {
  int num_endpoints = 500;
  int num_connections = 20000;
  int64_t now = 2000;
  int64_t time_at_last_scrape = 1000;
  int64_t afterglow_period_micros = 20000000;  // 20 seconds in microseconds
  ConnMap new_state, old_state, delta;

  CreateFakeState(new_state, num_endpoints, num_connections, time_at_last_scrape);
  auto t1 = std::chrono::steady_clock::now();
  CT::ComputeDeltaAfterglow(new_state, old_state, delta, now, time_at_last_scrape, afterglow_period_micros);
  auto t2 = std::chrono::steady_clock::now();
  std::chrono::duration<double, std::milli> dur = t2 - t1;
  std::cout << "old_state.size()= " << old_state.size() << std::endl;
  std::cout << "new_state.size()= " << new_state.size() << std::endl;
  std::cout << "Time taken by ComputeDeltaAfterglow= " << dur.count() << " ms\n";
}

TEST(ConnTrackerTest, TestComputeDeltaAfterglowBenchmark2) {
  int num_endpoints = 500;
  int num_connections = 20000;
  int64_t now = 2000;
  int64_t time_at_last_scrape = 1000;
  int64_t afterglow_period_micros = 20000000;  // 20 seconds in microseconds
  ConnMap new_state, old_state, delta;

  CreateFakeState(new_state, num_endpoints, num_connections, now);
  CreateFakeState(new_state, num_endpoints, num_connections, time_at_last_scrape);
  auto t1 = std::chrono::steady_clock::now();
  CT::ComputeDeltaAfterglow(new_state, old_state, delta, now, time_at_last_scrape, afterglow_period_micros);
  auto t2 = std::chrono::steady_clock::now();
  std::chrono::duration<double, std::milli> dur = t2 - t1;
  std::cout << "old_state.size()= " << old_state.size() << std::endl;
  std::cout << "new_state.size()= " << new_state.size() << std::endl;
  std::cout << "Time taken by ComputeDeltaAfterglow= " << dur.count() << " ms\n";
}

}  // namespace

}  // namespace collector
