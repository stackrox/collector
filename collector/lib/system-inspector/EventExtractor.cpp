#include "EventExtractor.h"

namespace collector::system_inspector {

void EventExtractor::Init(sinsp* inspector) {
  for (auto* wrapper : wrappers_) {
    std::unique_ptr<sinsp_filter_check> check = FilterList().new_filter_check_from_fldname(wrapper->event_name, inspector, true);
    check->parse_field_name(wrapper->event_name, true, false);
    wrapper->filter_check.reset(check.release());
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

std::ostream& operator<<(std::ostream& os, const sinsp_threadinfo* t) {
  if (t) {
    os << "Container: \"" << t->m_container_id << "\", Name: " << t->m_comm << ", PID: " << t->m_pid << ", Args: " << t->m_exe;
  } else {
    os << "NULL\n";
  }
  return os;
}

}  // namespace collector::system_inspector
