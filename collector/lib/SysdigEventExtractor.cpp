
#include "SysdigEventExtractor.h"

#include "Logging.h"

namespace collector {

void SysdigEventExtractor::Init(sinsp* inspector) {
  for (auto* wrapper : wrappers_) {
    sinsp_filter_check* check = g_filterlist.new_filter_check_from_fldname(wrapper->event_name, inspector, true);
    check->parse_field_name(wrapper->event_name, true, false);
    wrapper->filter_check.reset(check);
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
