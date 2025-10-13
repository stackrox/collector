#pragma once

#include <memory>

#include "internalapi/sensor/signal_iservice.pb.h"
#include "storage/process_indicator.pb.h"

#include "ProtoAllocator.h"
#include "Utility.h"
#include "events/IEvent.h"
#include "events/pipeline/Nodes.h"

namespace collector::pipeline {

class ProcessProtoTransformer : public Transformer<events::ProcessStartEvent, std::shared_ptr<sensor::SignalStreamMessage>> {
 public:
  ProcessProtoTransformer(
      const std::shared_ptr<Queue<events::ProcessStartEvent>>& input,
      const std::shared_ptr<Queue<std::shared_ptr<sensor::SignalStreamMessage>>>& output)
      : Transformer<events::ProcessStartEvent, std::shared_ptr<sensor::SignalStreamMessage>>(input, output) {}

  std::optional<std::shared_ptr<sensor::SignalStreamMessage>> transform(const events::ProcessStartEvent& event) {
    storage::ProcessSignal* proc_signal = allocator_.Allocate<storage::ProcessSignal>();

    if (!proc_signal) {
      return std::nullopt;
    }

    proc_signal->set_id(UUIDStr());

    auto process = event.Process();

    proc_signal->set_name(process->name());
    proc_signal->set_exec_file_path(process->exe_path());
    proc_signal->set_container_id(process->container_id());
    proc_signal->set_pid(process->pid());
    proc_signal->set_args(process->args());

    sensor::SignalStreamMessage* msg = allocator_.AllocateRoot();
    v1::Signal* signal = allocator_.Allocate<v1::Signal>();

    signal->set_allocated_process_signal(proc_signal);

    msg->clear_collector_register_request();
    msg->set_allocated_signal(signal);

    std::shared_ptr<sensor::SignalStreamMessage> ptr;
    ptr.reset(msg);
    return {ptr};
  }

 private:
  ProtoAllocator<sensor::SignalStreamMessage> allocator_;
};

}  // namespace collector::pipeline
