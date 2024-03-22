#include "EventExtractor.h"

namespace collector::system_inspector {

void EventExtractor::Init(sinsp* inspector) {
  for (auto* wrapper : wrappers_) {
    sinsp_filter_check* check = g_filterlist.new_filter_check_from_fldname(wrapper->event_name, inspector, true);
    check->parse_field_name(wrapper->event_name, true, false);
    wrapper->filter_check.reset(check);
  }
}

void EventExtractor::ClearWrappers() {
  for (FilterCheckWrapper* wrapper : wrappers_) {
    if (wrapper) {
      wrapper->filter_check.reset();
    }
  }
  wrappers_.clear();
}

}  // namespace collector::system_inspector
