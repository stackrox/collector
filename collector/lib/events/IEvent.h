#pragma once

#include <iostream>
#include <memory>

#include "Process.h"

namespace collector::events {

enum EventType {
  ProcessStart,
  NetworkConnection,
};

class IEvent {
 public:
  virtual ~IEvent() = default;

  virtual EventType Type() const = 0;
};

using IEventPtr = std::shared_ptr<const IEvent>;

class ProcessStartEvent : public IEvent {
 public:
  ProcessStartEvent(const std::shared_ptr<IProcess>& process) : process_(process) {}

  EventType Type() const {
    return EventType::ProcessStart;
  }

  friend std::ostream& operator<<(std::ostream& stream, const ProcessStartEvent& event) {
    stream << *event.Process();
    return stream;
  }

  const std::shared_ptr<IProcess>& Process() const {
    return process_;
  }

 private:
  std::shared_ptr<IProcess> process_;
};

}  // namespace collector::events
