//
// Created by Malte Isberner on 9/11/18.
//

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

  auto state = tracker.FetchState();
  EXPECT_THAT(state, UnorderedElementsAre(std::make_pair(conn1, CT::MakeActive(now)), std::make_pair(conn2, CT::MakeActive(now))));

  auto state2 = tracker.FetchState();
  EXPECT_EQ(state, state2);

  int64_t now2 = NowMicros();
  tracker.RemoveConnection(conn1, now2);
  state = tracker.FetchState();
  EXPECT_THAT(state, UnorderedElementsAre(std::make_pair(conn1, CT::MakeInactive(now2)), std::make_pair(conn2, CT::MakeActive(now))));

  state = tracker.FetchState();
  EXPECT_THAT(state, UnorderedElementsAre(std::make_pair(conn2, CT::MakeActive(now))));
}


TEST(ConnTrackerTest, TestUpdate) {
  Endpoint a(Address(192, 168, 0, 1), 80);
  Endpoint b(Address(192, 168, 1, 10), 9999);

  Connection conn1("xyz", a, b, L4Proto::TCP, true);
  Connection conn2("xzy", b, a, L4Proto::TCP, false);

  int64_t now = NowMicros();

  ConnectionTracker tracker;
  tracker.Update({conn1, conn2}, now);

  auto state = tracker.FetchState();
  EXPECT_THAT(state, UnorderedElementsAre(std::make_pair(conn1, CT::MakeActive(now)), std::make_pair(conn2, CT::MakeActive(now))));

  auto state2 = tracker.FetchState();
  EXPECT_EQ(state, state2);

  int64_t now2 = NowMicros();
  tracker.Update({conn1}, now2);
  state = tracker.FetchState();
  EXPECT_THAT(state, UnorderedElementsAre(std::make_pair(conn1, CT::MakeActive(now2)), std::make_pair(conn2, CT::MakeInactive(now))));

  state = tracker.FetchState();
  EXPECT_THAT(state, UnorderedElementsAre(std::make_pair(conn1, CT::MakeActive(now2))));
}

TEST(ConnTrackerTest, TestComputeDelta) {
  Endpoint a(Address(192, 168, 0, 1), 80);
  Endpoint b(Address(192, 168, 1, 10), 9999);

  Connection conn1("xyz", a, b, L4Proto::TCP, true);
  Connection conn2("xzy", b, a, L4Proto::TCP, false);

  int64_t now = NowMicros();

  ConnMap orig_state = {{conn1, CT::MakeActive(now)}, {conn2, CT::MakeActive(now)}};
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
  EXPECT_THAT(state2, UnorderedElementsAre(std::make_pair(conn1, CT::MakeInactive(now))));

  // Just updating the timestamp of an active connection should not make it appear in the delta.
  state1 = state2 = orig_state;
  state1[conn1] = CT::MakeActive(NowMicros());
  CT::ComputeDelta(state1, &state2);
  EXPECT_THAT(state2, IsEmpty());

  // Marking a connection as inactive should make it appear as inactive in the delta.
  state1 = state2 = orig_state;
  int64_t now2 = NowMicros();
  state1[conn1] = CT::MakeInactive(now2);
  CT::ComputeDelta(state1, &state2);
  EXPECT_THAT(state2, UnorderedElementsAre(std::make_pair(conn1, CT::MakeInactive(now2))));

  // A connection that was already inactive no longer showing up at all in the new state should not appear in the delta.
  state1 = state2 = orig_state;
  CT::MakeInactive(&state2[conn1]);
  state1.erase(conn1);
  CT::ComputeDelta(state1, &state2);
  EXPECT_THAT(state2, IsEmpty());
}

}  // namespace

}  // namespace collector
