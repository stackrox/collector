#ifndef _EVENT_NAMES_H_
#define _EVENT_NAMES_H_

#include <array>
#include <string>
#include <unordered_map>
#include <vector>

#include "CollectorException.h"
#include "ppm_events_public.h"

namespace collector {

class EventNames {
 public:
  using EventIDVector = std::vector<ppm_event_type>;

  static const EventNames& GetInstance();

  const EventIDVector& GetEventIDs(const std::string& name) const {
    auto it = events_by_name_.find(name);
    if (it == events_by_name_.end()) {
      throw CollectorException("Invalid event name '" + name + "'");
    }
    return it->second;
  }

  // Return event name for given event id
  const std::string& GetEventName(uint16_t id) const {
    if (id < 0 || id >= names_by_id_.size()) {
      throw CollectorException("Invalid event id " + std::to_string(id));
    }
    return names_by_id_[id];
  }

  // Return associated syscall id for given event id
  uint16_t GetEventSyscallID(uint16_t id) const {
    if (id < 0 || id >= syscall_by_id_.size()) {
      throw CollectorException("Invalid event id " + std::to_string(id));
    }
    return syscall_by_id_[id];
  }

 private:
  EventNames();

  std::unordered_map<std::string, EventIDVector> events_by_name_;
  std::array<std::string, PPM_EVENT_MAX> names_by_id_;
  std::array<uint16_t, PPM_EVENT_MAX> syscall_by_id_;
};

}  // namespace collector

#endif  // _EVENT_NAMES_H_
