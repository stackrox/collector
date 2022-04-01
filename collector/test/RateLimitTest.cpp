#include <chrono>
#include <thread>

#include "RateLimit.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace collector {
namespace {

TEST(RateLimitTest, TokenBucket) {
  // Limit 10 burst size, refill every 5 seconds
  Limiter l(10, 5);
  TokenBucket b;
  EXPECT_EQ(l.Tokens(&b), 10);
  EXPECT_EQ(l.AllowN(&b, 9), true);
  EXPECT_EQ(l.Tokens(&b), 1);
  EXPECT_EQ(l.AllowN(&b, 1), true);
  EXPECT_EQ(l.Tokens(&b), 0);

  EXPECT_EQ(l.AllowN(&b, 1), false);

  // sleep 5
  std::this_thread::sleep_for(std::chrono::seconds(6));
  EXPECT_EQ(l.Tokens(&b), 10);
  EXPECT_EQ(l.AllowN(&b, 1), true);
  EXPECT_EQ(l.Tokens(&b), 9);
}

TEST(RateLimitTest, CacheTest) {
  RateLimitCache r(2, 2, 5);
  EXPECT_EQ(r.Allow("A"), true);
  EXPECT_EQ(r.Allow("A"), true);
  EXPECT_EQ(r.Allow("B"), true);
  EXPECT_EQ(r.Allow("B"), true);
  EXPECT_EQ(r.Allow("A"), false);
  EXPECT_EQ(r.Allow("B"), false);

  std::this_thread::sleep_for(std::chrono::seconds(6));

  EXPECT_EQ(r.Allow("A"), true);
  EXPECT_EQ(r.Allow("A"), true);
  EXPECT_EQ(r.Allow("B"), true);
  EXPECT_EQ(r.Allow("B"), true);
}

TEST(RateLimitTest, EvictionTest) {
  RateLimitCache r(2, 2, 5);
  EXPECT_EQ(r.Allow("A"), true);
  EXPECT_EQ(r.Allow("A"), true);
  EXPECT_EQ(r.Allow("B"), true);
  EXPECT_EQ(r.Allow("B"), true);
  EXPECT_EQ(r.Allow("A"), false);
  EXPECT_EQ(r.Allow("B"), false);

  EXPECT_EQ(r.Allow("C"), true);

  EXPECT_EQ(r.Allow("A"), true);
  EXPECT_EQ(r.Allow("A"), true);
  EXPECT_EQ(r.Allow("B"), true);
  EXPECT_EQ(r.Allow("B"), true);

  EXPECT_EQ(r.Allow("D"), true);

  EXPECT_EQ(r.Allow("A"), true);
  EXPECT_EQ(r.Allow("A"), true);
  EXPECT_EQ(r.Allow("B"), true);
  EXPECT_EQ(r.Allow("B"), true);
}

}  // namespace

}  // namespace collector
