#include "CollectorStats.h"

namespace collector {

#define X(n) #n,
std::array<std::string, CollectorStats::timer_type_max> CollectorStats::timer_type_to_name = {
  TIMER_NAMES
};
#undef X

#define X(n) #n,
std::array<std::string, CollectorStats::counter_type_max> CollectorStats::counter_type_to_name = {
  COUNTER_NAMES
};
#undef X

} // namespace collector
