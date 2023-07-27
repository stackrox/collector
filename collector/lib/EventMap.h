#ifndef _EVENT_MAP_H_
#define _EVENT_MAP_H_

#include <array>
#include <string>
#include <utility>

#include "EventNames.h"
#include "ppm_events_public.h"

namespace collector {

template <typename T>
class EventMap {
 public:
  EventMap() : EventMap(T()) {}
  EventMap(const T& initVal) : event_names_(EventNames::GetInstance()) {
    values_.fill(initVal);
  }
  EventMap(std::initializer_list<std::pair<std::string, T>> init, const T& initVal = T()) : EventMap(initVal) {
    for (const auto& pair : init) {
      Set(pair.first, pair.second);
    }
  }

  T& operator[](uint16_t id) {
    if (id < 0 || id >= values_.size()) {
      throw CollectorException("Invalid event id " + std::to_string(id));
    }
    return values_[id];
  }

  const T& operator[](uint16_t id) const {
    if (id < 0 || id >= values_.size()) {
      throw CollectorException("Invalid event id " + std::to_string(id));
    }
    return values_[id];
  }

  void Set(const std::string& name, const T& value) {
    for (auto event_id : event_names_.GetEventIDs(name)) {
      values_[event_id] = value;
    }
  }

 private:
  const EventNames& event_names_;
  std::array<T, PPM_EVENT_MAX> values_;
};

}  // namespace collector

#endif  // _EVENT_MAP_H_
