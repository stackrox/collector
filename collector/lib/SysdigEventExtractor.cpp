
#include "SysdigEventExtractor.h"

#include "Logging.h"

namespace collector {

void SysdigEventExtractor::Init(sinsp* inspector) {
  for (auto* wrapper : wrappers_) {
    wrapper->filter_check.reset(sinsp_filter_check_iface::get(wrapper->event_name, inspector));
  }
}

void SysdigEventExtractor::ClearWrappers() {
  for (FilterCheckWrapper* wrapper : wrappers_) {
    if (wrapper) {
      wrapper->filter_check.reset();
    }
  }
  wrappers_.clear();
}

}  // namespace collector
