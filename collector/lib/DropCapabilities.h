#ifndef _DROP_CAPABILITIES_H_
#define _DROP_CAPABILITIES_H_

#include <initializer_list>

extern "C" {
#include <cap-ng.h>
}

#include "Logging.h"

namespace collector {

// Drop all Linux capabilities except those specified.
// Clears the bounding set too, preventing regain via execve.
// Logs the result but does not abort on failure.
inline void DropCapabilities(std::initializer_list<unsigned int> keep) {
  capng_clear(CAPNG_SELECT_ALL);

  auto caps = static_cast<capng_type_t>(CAPNG_EFFECTIVE | CAPNG_PERMITTED);
  for (auto cap : keep) {
    capng_update(CAPNG_ADD, caps, cap);
  }

  if (capng_apply(CAPNG_SELECT_ALL) != 0) {
    CLOG(WARNING) << "Failed to drop capabilities";
  }
}

}  // namespace collector

#endif  // _DROP_CAPABILITIES_H_
