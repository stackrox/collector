#include "CollectorStats.h"

namespace collector {

#define X(n) #n,
std::array<std::string, CollectorStats::timer_type_max> CollectorStats::timer_type_to_name = {
    TIMER_NAMES};
#undef X

#define X(n) #n,
std::array<std::string, CollectorStats::counter_type_max> CollectorStats::counter_type_to_name = {
    COUNTER_NAMES};
#undef X

CollectorStats* CollectorStats::GetOrCreate() {
  if (!CollectorStats::stats_) CollectorStats::stats_ = new CollectorStats;

  return CollectorStats::stats_;
}

void CollectorStats::Reset() {
  for (int i = 0; i < timer_type_max; i++) {
    CollectorStats::stats_->timer_count_[i] = 0;
    CollectorStats::stats_->timer_total_us_[i] = 0;
  }
  for (int i = 0; i < counter_type_max; i++) {
    CollectorStats::stats_->counter_[i] = 0;
  }
}

CollectorStats* CollectorStats::stats_ = NULL;

}  // namespace collector
