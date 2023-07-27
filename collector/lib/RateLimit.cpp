#include "RateLimit.h"

#include "CollectorStats.h"
#include "Logging.h"
#include "TimeUtil.h"
#include "Utility.h"

namespace collector {

Limiter::Limiter(int64_t burst_size, int64_t refill_time) {
  using namespace std::chrono;
  burst_size_ = burst_size;
  refill_time_ = duration_cast<microseconds>(seconds(refill_time)).count();
}

bool Limiter::Allow(TokenBucket* b) {
  return AllowN(b, 1);
}

int64_t Limiter::Tokens(TokenBucket* b) {
  return std::min(burst_size_, b->tokens + (refill_count(b) * burst_size_));
}

bool Limiter::AllowN(TokenBucket* b, int64_t n) {
  if (!b->last_time) fill_bucket(b);

  int64_t refill = refill_count(b);
  b->tokens += refill * burst_size_;
  b->last_time += refill * refill_time_;

  if (b->tokens >= burst_size_) fill_bucket(b);

  if (n > b->tokens) return false;

  b->tokens -= n;

  return true;
}

int64_t Limiter::refill_count(TokenBucket* b) {
  return (NowMicros() - b->last_time) / refill_time_;
}

void Limiter::fill_bucket(TokenBucket* b) {
  b->last_time = NowMicros();
  b->tokens = burst_size_;
}

// RateLimitCache Defaults: Limit duplicate events to rate of 10 every 30 min
RateLimitCache::RateLimitCache()
    : capacity_(4096), limiter_(new Limiter(10, 30 * 60)) {}

RateLimitCache::RateLimitCache(size_t capacity, int64_t burst_size, int64_t refill_time)
    : capacity_(capacity), limiter_(new Limiter(burst_size, refill_time)) {}

void RateLimitCache::ResetRateLimitCache() {
  limiter_.reset();
}

bool RateLimitCache::Allow(std::string key) {
  auto pair = cache_.emplace(std::make_pair(key, TokenBucket()));
  if (pair.second && cache_.size() > capacity_) {
    CLOG(INFO) << "Flushing rate limiting cache";
    cache_.clear();
    COUNTER_INC(CollectorStats::rate_limit_flushing_counts);
    return true;
  }
  return limiter_->Allow(&pair.first->second);
}

}  // namespace collector
