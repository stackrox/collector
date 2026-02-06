#pragma once

#include "events/IEvent.h"
#include "events/handlers/Handler.h"
#include "events/pipeline/Pipeline.h"
#include "events/pipeline/nodes/ProcessProtoTransformer.h"
#include "events/pipeline/nodes/SignalServiceConsumer.h"

namespace collector::events::handler {

class ProcessHandler : public EventHandler<ProcessHandler, ProcessStartEvent> {
 public:
  using EventType = ProcessStartEvent;

  using ProcessPipeline = collector::pipeline::Pipeline<
      pipeline::ProcessProtoTransformer,
      pipeline::SignalServiceConsumer>;

  ProcessHandler() {
    process_pipeline_.Start();
  }

  ~ProcessHandler() {
    process_pipeline_.Stop();
  }

  void HandleImpl(const ProcessStartEvent& event) const {
    process_pipeline_.Push(event);
  }

 private:
  ProcessPipeline process_pipeline_;
};

}  // namespace collector::events::handler
