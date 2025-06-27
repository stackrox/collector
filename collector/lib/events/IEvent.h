#pragma once

#include <memory>

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
  EventType Type() const {
    return EventType::ProcessStart;
  }
};

}  // namespace collector::events
