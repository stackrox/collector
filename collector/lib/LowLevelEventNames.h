#ifndef _LOW_LEVEL_EVENT_NAMES_H_
#define _LOW_LEVEL_EVENT_NAMES_H_

#include <array>
#include <string>

#include "ppm_events_public.h"

namespace collector {

class LowLevelEventNames {
 public:
  static const LowLevelEventNames& GetInstance();

  const std::string& GetEventName(ppm_event_type event_type) const;

 private:
  LowLevelEventNames();

  std::array<std::string, PPM_EVENT_MAX> names_;
};
}

#endif  // _LOW_LEVEL_EVENT_NAMES_H_
