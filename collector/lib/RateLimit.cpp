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

#include "RateLimit.h"
#include "Utility.h"
#include "TimeUtil.h"
#include "Logging.h"

namespace collector {

Limiter::Limiter(int64_t burst_size, int64_t refill_time) {
  using namespace std::chrono;
  burst_size_ = burst_size;
  refill_time_ = duration_cast<microseconds>(seconds(refill_time)).count();
}

bool Limiter::Allow(TokenBucket *b) {
  return AllowN(b, 1);
}

int64_t Limiter::Tokens(TokenBucket *b) {
  int64_t tokens = b->tokens + (time_delta(b) / refill_time_) * burst_size_;
  return std::min(burst_size_, tokens);
}

bool Limiter::AllowN(TokenBucket *b, int64_t n) {
  int64_t delta = time_delta(b);
  b->tokens += (delta / refill_time_) * burst_size_;
  b->last_time += delta;

  if (b->tokens >= burst_size_) fill_bucket(b);

  if (n > b->tokens) return false;

  b->tokens -= n;

  return true;
}

int64_t Limiter::time_delta(TokenBucket *b) {
  return NowMicros() - b->last_time;
}

void Limiter::fill_bucket(TokenBucket *b) {
  b->last_time = NowMicros();
  b->tokens = burst_size_;
}

// RateLimitCache Defaults: Limit duplicate events to rate of 10 every 30 min
RateLimitCache::RateLimitCache() 
  : capacity_(4096), limiter_(new Limiter(10, 30*60)) {}

RateLimitCache::RateLimitCache(size_t capacity, int64_t burst_size, int64_t refill_time)
  : capacity_(capacity), limiter_(new Limiter(burst_size, refill_time)) {}

bool RateLimitCache::Allow(std::string key) { 
  auto pair = cache_.emplace(std::make_pair(key, TokenBucket()));
  if (pair.second && cache_.size() > capacity_) {
    CLOG(INFO) << "Flushing rate limiting cache";
    cache_.clear();
    return true;
  }
  return limiter_->Allow(&pair.first->second);
}

}  // namespace collector
