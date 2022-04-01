#include "EventNames.h"

extern const struct ppm_event_info g_event_info[];       // defined in libscap
extern const struct syscall_evt_pair g_syscall_table[];  // defined in libscap

namespace collector {

const EventNames& EventNames::GetInstance() {
  static EventNames* event_names = new EventNames;
  return *event_names;
}

EventNames::EventNames() {
  for (int i = 0; i < PPM_EVENT_MAX; i++) {
    std::string name(g_event_info[i].name);
    syscall_by_id_[i] = 0;
    names_by_id_[i] = name;
    ppm_event_type event_type(static_cast<ppm_event_type>(i));
    events_by_name_[name].push_back(event_type);
    if (PPME_IS_ENTER(event_type)) {
      events_by_name_[name + ">"].push_back(event_type);
    } else {
      events_by_name_[name + "<"].push_back(event_type);
    }
  }
  for (int i = 0; i < SYSCALL_TABLE_SIZE; i++) {
    ppm_event_type enter_evt = g_syscall_table[i].enter_event_type;
    if (enter_evt < 0 || enter_evt >= syscall_by_id_.size()) {
      throw CollectorException("Invalid syscall event id " + std::to_string(enter_evt));
    }
    syscall_by_id_[enter_evt] = i;

    ppm_event_type exit_evt = g_syscall_table[i].exit_event_type;
    if (exit_evt < 0 || exit_evt >= syscall_by_id_.size()) {
      throw CollectorException("Invalid syscall event id " + std::to_string(exit_evt));
    }
    syscall_by_id_[exit_evt] = i;
  }
}

}  // namespace collector
