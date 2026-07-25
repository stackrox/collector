#ifndef _DROP_CAPABILITIES_H_
#define _DROP_CAPABILITIES_H_

#include <initializer_list>

extern "C" {
#include <cap-ng.h>
}

#include "Logging.h"

namespace collector {

// Drop all Linux capabilities except those specified.
// If clear_bounding is true, also clears the bounding set (requires
// CAP_SETPCAP — use only on the first drop before other caps are lost).
// Logs the result but does not abort on failure.
inline void DropCapabilities(std::initializer_list<unsigned int> keep,
                             bool clear_bounding = false) {
  auto scope = clear_bounding ? CAPNG_SELECT_ALL : CAPNG_SELECT_CAPS;
  capng_clear(scope);

  auto caps = static_cast<capng_type_t>(CAPNG_EFFECTIVE | CAPNG_PERMITTED);
  for (auto cap : keep) {
    capng_update(CAPNG_ADD, caps, cap);
  }

  if (capng_apply(scope) != 0) {
    CLOG(WARNING) << "Failed to drop capabilities";
  }
}

}  // namespace collector

#endif  // _DROP_CAPABILITIES_H_
