#ifndef _RATE_LIMIT_H_
#define _RATE_LIMIT_H_

#include <unordered_map>

#include "Utility.h"

namespace collector {

struct TokenBucket {
  TokenBucket() : tokens(0), last_time(0) {}
  int64_t tokens;     // number of tokens in this bucket
  int64_t last_time;  // amount of time since the bucket last updated in microseconds
};

class Limiter {
 public:
  Limiter(int64_t burst_size, int64_t refill_time);
  bool Allow(TokenBucket* b);
  bool AllowN(TokenBucket* b, int64_t n);
  int64_t Tokens(TokenBucket* b);

  void SetBurstSize(int64_t burst_size) {
    burst_size_ = burst_size;
  }

  void SetRefillTime(int64_t refill) {
    refill_time_ = refill;
  }

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

  void SetCapacity(size_t capacity) {
    capacity_ = capacity;
  }

  void SetBurstSize(int64_t burst_size) {
    limiter_->SetBurstSize(burst_size);
  }

  void SetRefillTime(int64_t refill) {
    limiter_->SetRefillTime(refill);
  }

 private:
  size_t capacity_;
  std::unique_ptr<Limiter> limiter_;
  std::unordered_map<std::string, TokenBucket> cache_;
};
}  // namespace collector

#endif  // _RATE_LIMIT_H_
