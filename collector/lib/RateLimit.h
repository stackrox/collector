#pragma once

#include <unordered_map>

#include "Utility.h"

namespace collector {

// Token bucket for time-based rate limiting. Each bucket tracks its own
// token count and last-refill timestamp.
struct TokenBucket {
  TokenBucket() : tokens(0), last_time(0) {}
  int64_t tokens;     // number of tokens in this bucket
  int64_t last_time;  // timestamp of last refill in microseconds since epoch
};

/// Time-based rate limiter using the token bucket algorithm. Used by
/// ProcessSignalHandler to deduplicate repeated process exec signals
/// (e.g., cron jobs or health checks that fire the same command constantly).
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

/// Per-key rate limiter with an LRU-style cache of token buckets.
/// Used for process signal deduplication, keyed by container+name+args+path.
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

/// Simple per-key count limiter (no time decay). Used to cap the number of
/// network connection events sent per container per scrape interval, to
/// prevent high-cardinality containers (especially with external IPs enabled)
/// from overwhelming Sensor. Close events are never rate-limited to avoid
/// zombie connections in Sensor's state. (ROX-24945)
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
