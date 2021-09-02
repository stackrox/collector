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

#ifndef _RATE_LIMIT_H_
#define _RATE_LIMIT_H_

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

 private:
  void fill_bucket(TokenBucket* b) const;
  int64_t refill_count(TokenBucket* b) const;

  int64_t burst_size_;   // max number of tokens per bucket and the number added each refill
  int64_t refill_time_;  // amount of time between refill in microseconds
};

class RateLimitCache {
 public:
  RateLimitCache();
  RateLimitCache(size_t capacity, int64_t burst_size, int64_t refill_time);
  bool Allow(std::string key);

 private:
  size_t capacity_;
  std::unique_ptr<Limiter> limiter_;
  unordered_map<std::string, TokenBucket> cache_;
};
}  // namespace collector

#endif  // _RATE_LIMIT_H_
