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

#include "ConnTracker.h"

#include <utility>

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "TimeUtil.h"

namespace collector {

namespace {

using CT = ConnectionTracker;
using ::testing::UnorderedElementsAre;
using ::testing::IsEmpty;

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

TEST(ConnTrackerTest, TestUpdateNormalized) {
  Endpoint a(Address(192, 168, 0, 1), 80);
  Endpoint b(Address(192, 168, 1, 10), 9999);
  Endpoint c(Address(192, 168, 1, 10), 54321);

  Connection conn1("xyz", a, b, L4Proto::TCP, true);
  Connection conn2("xzy", b, a, L4Proto::TCP, false);
  Connection conn3("xyz", a, c, L4Proto::TCP, true);
  Connection conn4("xzy", c, a, L4Proto::TCP, false);

  Connection conn13_normalized("xyz", Endpoint(Address(), 80), Endpoint(Address(192, 168, 1, 10), 0), L4Proto::TCP, true);
  Connection conn24_normalized("xzy", Endpoint(), a, L4Proto::TCP, false);

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
}

TEST(ConnTrackerTest, TestUpdateNormalizedExternal) {
  Endpoint a(Address(10, 1, 1, 8), 9999);
  Endpoint b(Address(35, 127, 0, 15), 54321);
  Endpoint c(Address(139, 14, 171, 3), 54321);
  Endpoint d(Address(35, 127, 4, 15), 54321);

  Connection conn1("xyz", a, b, L4Proto::TCP, true);
  Connection conn2("xyz", a, b, L4Proto::TCP, false);
  Connection conn3("xyz", a, c, L4Proto::TCP, true);
  Connection conn4("xyz", a, c, L4Proto::TCP, false);
  Connection conn5("xyz", a, d, L4Proto::TCP, false);

  Connection conn13_normalized("xyz", Endpoint(Address(), 9999), Endpoint(Address(255, 255, 255, 255), 0), L4Proto::TCP, true);
  Connection conn24_normalized("xyz", Endpoint(), Endpoint(Address(255, 255, 255, 255), 54321), L4Proto::TCP, false);

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

  Connection conn1_normalized("xyz", Endpoint(Address(), 9999), Endpoint(Address(35, 127, 0, 15), 0), L4Proto::TCP, true);
  Connection conn2_normalized("xyz", Endpoint(), b, L4Proto::TCP, false);
  Connection conn3_normalized("xyz", Endpoint(Address(), 9999), Endpoint(Address(255, 255, 255, 255), 0), L4Proto::TCP, true);
  Connection conn4_normalized("xyz", Endpoint(), Endpoint(Address(255, 255, 255, 255), 54321), L4Proto::TCP, false);
  Connection conn5_normalized("xyz", Endpoint(Address(), 9999), Endpoint(Address(35, 127, 1, 15), 0), L4Proto::TCP, true);

EXPECT_THAT(state3, UnorderedElementsAre(
          std::make_pair(conn1_normalized, ConnStatus(now, true)),
          std::make_pair(conn2_normalized, ConnStatus(now, true)),
          std::make_pair(conn3_normalized, ConnStatus(now, true)),
          std::make_pair(conn4_normalized, ConnStatus(now, true)),
          std::make_pair(conn5_normalized, ConnStatus(now, true))));
}

TEST(ConnTrackerTest, TestComputeDelta) {
  Endpoint a(Address(192, 168, 0, 1), 80);
  Endpoint b(Address(192, 168, 1, 10), 9999);

  Connection conn1("xyz", a, b, L4Proto::TCP, true);
  Connection conn2("xzy", b, a, L4Proto::TCP, false);

  int64_t now = NowMicros();

  ConnMap orig_state = {{conn1, ConnStatus(now, true)}, {conn2, ConnStatus(now, true)}};
  ConnMap state1 = orig_state;
  ConnMap state2;

  // ComputeDelta on an empty old state should just copy over the entire state.
  CT::ComputeDelta(state1, &state2);
  EXPECT_EQ(state1, state2);

  // ComputeDelta on two equal states should result in an empty delta.
  CT::ComputeDelta(state1, &state2);
  EXPECT_THAT(state2, IsEmpty());

  // Removing a connection from the active state should have it appear as inactive in the delta (with the previous
  // timestamp).
  state2 = state1;
  state1.erase(conn1);
  CT::ComputeDelta(state1, &state2);
  EXPECT_THAT(state2, UnorderedElementsAre(std::make_pair(conn1, ConnStatus(now, false))));

  // Just updating the timestamp of an active connection should not make it appear in the delta.
  state1 = state2 = orig_state;
  state1[conn1] = ConnStatus(NowMicros(), true);
  CT::ComputeDelta(state1, &state2);
  EXPECT_THAT(state2, IsEmpty());

  // Marking a connection as inactive should make it appear as inactive in the delta.
  state1 = state2 = orig_state;
  int64_t now2 = NowMicros();
  state1[conn1] = ConnStatus(now2, false);
  CT::ComputeDelta(state1, &state2);
  EXPECT_THAT(state2, UnorderedElementsAre(std::make_pair(conn1, ConnStatus(now2, false))));

  // A connection that was already inactive no longer showing up at all in the new state should not appear in the delta.
  state1 = state2 = orig_state;
  state2[conn1].SetActive(false);
  state1.erase(conn1);
  CT::ComputeDelta(state1, &state2);
  EXPECT_THAT(state2, IsEmpty());
}

}  // namespace

}  // namespace collector
