#pragma once

#include <queue>

// clang-format: off
#include "events/pipeline/nodes/Debug.h"
// clang-format: on

#include "events/IEvent.h"
#include "events/handlers/Handler.h"
#include "events/pipeline/Pipeline.h"

namespace collector::events::handler {

using ProcessPipeline = collector::pipeline::Pipeline<pipeline::DebugNode<ProcessStartEvent>>;

class ProcessHandler : public EventHandler<ProcessHandler, ProcessStartEvent> {
 public:
  using EventType = ProcessStartEvent;

  ProcessHandler() {}

  void HandleImpl(const ProcessStartEvent& event) const {
    process_pipeline_.Push(event);
  }

 private:
  ProcessPipeline process_pipeline_;
};

}  // namespace collector::events::handler
