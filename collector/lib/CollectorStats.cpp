#include "CollectorStats.h"

#include <iostream>

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
  if (CollectorStats::stats_) delete CollectorStats::stats_;
  CollectorStats::stats_ = 0;
}

CollectorStats* CollectorStats::stats_ = NULL;

}  // namespace collector
