#ifndef COLLECTOR_COLLECTORSTATS_H
#define COLLECTOR_COLLECTORSTATS_H

#include <atomic>
#include <unordered_map>
#include <vector>

#include "TimeUtil.h"

#define TIMER_NAMES     \
  X(net_scrape_read)    \
  X(net_scrape_update)  \
  X(net_fetch_state)    \
  X(net_create_message) \
  X(net_write_message)

#define COUNTER_NAMES          \
  X(net_conn_updates)          \
  X(net_conn_deltas)           \
  X(net_conn_inactive)         \
  X(net_cep_updates)           \
  X(net_cep_deltas)            \
  X(net_cep_inactive)          \
  X(net_known_ip_networks)     \
  X(net_known_public_ips)      \
  X(process_lineage_counts)    \
  X(process_lineage_total)     \
  X(process_lineage_sqr_total) \
  X(process_lineage_string_total)
namespace collector {

//This is a singleton class which keeps track of metrics
class CollectorStats {
 public:
  CollectorStats(const CollectorStats&) = delete;
  CollectorStats& operator=(const CollectorStats&) = delete;
  CollectorStats(CollectorStats&&) = delete;

#define X(n) n,
  enum TimerType {
    TIMER_NAMES
        X(timer_type_max)
  };
#undef X
  static std::array<std::string, timer_type_max> timer_type_to_name;

  static CollectorStats& GetOrCreate();
  static void Reset();
  inline int64_t GetTimerCount(size_t index) const { return timer_count_[index]; }
  inline int64_t GetTimerDurationMicros(size_t index) const { return timer_total_us_[index]; }
  inline void EndTimerAt(size_t index, int64_t duration_us) {
    ++timer_count_[index];
    timer_total_us_[index] += duration_us;
  }

#define X(n) n,
  enum CounterType {
    COUNTER_NAMES
        X(counter_type_max)
  };
#undef X
  static std::array<std::string, counter_type_max> counter_type_to_name;

  inline int64_t GetCounter(size_t index) const { return counter_[index]; }
  inline void CounterSet(size_t index, int64_t val) {
    counter_[index] = val;
  }
  inline void CounterAdd(size_t index, int64_t val) {
    counter_[index] += val;
  }

 private:
  std::array<std::atomic<int64_t>, timer_type_max> timer_count_ = {{}};
  std::array<std::atomic<int64_t>, timer_type_max> timer_total_us_ = {{}};

  std::array<std::atomic<int64_t>, counter_type_max> counter_ = {{}};

  CollectorStats(){};
};

namespace internal {

template <typename T>
class ScopedTimer {
 public:
  ScopedTimer(T* timer_array, size_t index)
      : timer_array_(timer_array), index_(index), start_time_(NowMicros()) {}
  ~ScopedTimer() {
    if (timer_array_) {
      timer_array_->EndTimerAt(index_, NowMicros() - start_time_);
    }
  }
  constexpr operator bool() const { return true; }

 private:
  T* timer_array_;
  size_t index_;
  int64_t start_time_;
};

template <typename T>
ScopedTimer<T> scoped_timer(T* timer_array, size_t index) {
  return ScopedTimer<T>(timer_array, index);
}

}  // namespace internal

#define SCOPED_TIMER(i) auto __scoped_timer_##__LINE__ = internal::scoped_timer(&CollectorStats::GetOrCreate(), i)
#define WITH_TIMER(i) if (SCOPED_TIMER(i))

#define COUNTER_SET(i, v) CollectorStats::GetOrCreate().CounterSet(i, static_cast<int64_t>(v));
#define COUNTER_ADD(i, v) CollectorStats::GetOrCreate().CounterAdd(i, static_cast<int64_t>(v));

#define COUNTER_INC(i) COUNTER_ADD(i, 1)
#define COUNTER_ZERO(i) COUNTER_SET(i, 0)

}  // namespace collector

#endif  //COLLECTOR_COLLECTORSTATS_H
