#pragma once

#include <queue>

#include "events/IEvent.h"
#include "events/handlers/Handler.h"

namespace collector::events::handler {

class ProcessHandler : public EventHandler<ProcessHandler, ProcessStartEvent> {
 public:
  using EventType = ProcessStartEvent;

  ProcessHandler() {}

  void HandleImpl(const ProcessStartEvent& event) const {
    queue_->push(event);
  }

 private:
  std::shared_ptr<std::queue<ProcessStartEvent>> queue_;
};

}  // namespace collector::events::handler
