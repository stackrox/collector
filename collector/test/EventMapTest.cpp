#include <string>

#include "EventMap.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

TEST(EventMap, MissingElement) {
  using namespace collector;
  using ::testing::StartsWith;
  enum class TestModifier : uint8_t {
    INVALID = 0,
    SHUTDOWN,
  };
  EventMap<TestModifier> modifiers = {
      {
          {"shutdown<", TestModifier::SHUTDOWN},
      },
      TestModifier::INVALID,
  };

  EXPECT_EQ(TestModifier::SHUTDOWN, modifiers[PPME_SOCKET_SHUTDOWN_X]);
  EXPECT_EQ(TestModifier::INVALID, modifiers[PPME_SOCKET_SHUTDOWN_E]);
}
