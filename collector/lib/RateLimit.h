#pragma once

#include <unordered_map>

#include "Utility.h"

namespace collector {

struct TokenBucket {
  TokenBucket() : tokens(0), last_time(0) {}
  int64_t tokens;     // number of tokens in this bucket
  int64_t last_time;  // amount of time since the bucket last updated in microseconds
};

class TimeLimiter {
 public:
  TimeLimiter(int64_t burst_size, int64_t refill_time);
  bool Allow(TokenBucket* b);
  bool AllowN(TokenBucket* b, int64_t n);
  int64_t Tokens(TokenBucket* b);

 private:
  void fill_bucket(TokenBucket* b);
  int64_t refill_count(TokenBucket* b);

  int64_t burst_size_;   // max number of tokens per bucket and the number added each refill
  int64_t refill_time_;  // amount of time between refill in microseconds
};

class RateLimitCache {
 public:
  RateLimitCache();
  RateLimitCache(size_t capacity, int64_t burst_size, int64_t refill_time);
  void ResetRateLimitCache();
  bool Allow(std::string key);

 private:
  size_t capacity_;
  std::unique_ptr<TimeLimiter> limiter_;
  std::unordered_map<std::string, TokenBucket> cache_;
};

class CountLimiter {
 public:
  CountLimiter();
  CountLimiter(int64_t count);

  bool Allow(std::string key);

 private:
  int64_t count_;
  std::unordered_map<std::string, TokenBucket> cache_;
};
}  // namespace collector
