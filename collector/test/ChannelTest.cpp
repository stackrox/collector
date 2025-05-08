#include <thread>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "Channel.h"

namespace collector {
TEST(Basic, Basic) {
  auto channel = Channel<int>();
  channel << 4;
  int out = 0;
  channel >> out;
  EXPECT_EQ(out, 4);
}

TEST(Basic, Multiple) {
  auto channel = Channel<int>();
  channel
      << 4
      << 3
      << 2
      << 1
      << 0;
  for (int i = 4; i > 0; i--) {
    int out = 0;
    channel >> out;
    EXPECT_EQ(out, i);
  }
}

TEST(Basic, Iterleaved) {
  auto channel = Channel<int>(8);
  channel
      << 4
      << 3;
  int out = 0;
  channel >> out;
  EXPECT_EQ(out, 4);
  channel
      << 2
      << 1;

  for (int i = 3; i > 0; i--) {
    channel >> out;
    EXPECT_EQ(out, i);
  }

  channel
      << 10
      << 20;

  channel >> out;
  EXPECT_EQ(out, 10);

  channel >> out;
  EXPECT_EQ(out, 20);
}

TEST(Basic, String) {
  auto channel = Channel<std::string>(8);
  std::string should_move = "This should be moved";
  std::string should_copy = "This should be copied";
  channel
      << std::move(should_move)
      << should_copy;

  std::string out;
  channel >> out;
  EXPECT_EQ(out, "This should be moved");
  channel >> out;
  EXPECT_EQ(out, "This should be copied");
}

TEST(Multithread, NonBlocking) {
  auto channel = Channel<int>();
  auto th = std::thread([&channel] {
    for (int i = 0; i < 10; i++) {
      channel << i;
    }

    channel.Close();
  });

  int j = 0;
  for (auto i : channel) {
    EXPECT_EQ(i, j);
    j++;
  }

  EXPECT_EQ(j, 10);

  th.join();
}

TEST(Multithread, Blocking) {
  auto channel = Channel<int>(1);
  auto th = std::thread([&channel] {
    for (int i = 0; i < 10; i++) {
      channel << i;
    }

    channel.Close();
  });

  int j = 0;
  for (auto i : channel) {
    EXPECT_EQ(i, j);
    j++;
  }

  EXPECT_EQ(j, 10);

  th.join();
}

TEST(Multithread, CloseOnReader) {
  auto channel = Channel<int>(1);
  auto th = std::thread([&channel] {
    for (int i = 0; !channel.IsClosed(); i++) {
      channel << i;
    }
  });

  int j = 0;
  for (auto i : channel) {
    EXPECT_EQ(i, j);
    j++;

    if (j == 10) {
      channel.Close();
    }
  }

  EXPECT_EQ(j, 10);

  th.join();
}
}  // namespace collector
