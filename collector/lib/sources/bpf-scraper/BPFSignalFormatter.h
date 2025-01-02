#include <optional>
#include <string>

#include <uuid/uuid.h>

#include "api/v1/signal.pb.h"
#include "internalapi/sensor/signal_iservice.pb.h"
#include "storage/bpf.pb.h"

#include "ProtoAllocator.h"
#include "Utility.h"
#include "bpf/common.h"

namespace collector::sources {

class BPFSignalFormatter : protected ProtoAllocator<sensor::SignalStreamMessage> {
  using Signal = v1::Signal;
  using BpfSignal = storage::BpfSignal;

 public:
  sensor::SignalStreamMessage* ToProtoMessage(struct bpf_prog_result* result) {
    BpfSignal* bpf_signal = CreateBpfSignal(result);
    if (!bpf_signal) {
      return nullptr;
    }

    Signal* signal = Allocate<Signal>();
    signal->set_allocated_bpf_signal(bpf_signal);

    sensor::SignalStreamMessage* signal_stream_message = AllocateRoot();
    signal_stream_message->clear_collector_register_request();
    signal_stream_message->set_allocated_signal(signal);
    return signal_stream_message;
  }

 private:
  BpfSignal* CreateBpfSignal(struct bpf_prog_result* result) {
    BpfSignal* signal = Allocate<BpfSignal>();

    // signal->set_id(UUIDStr());
    const std::string name(result->name);
    const std::string attach_point(result->attached);

    signal->set_name(name);
    signal->set_attach_point(attach_point);

    return signal;
  }
};

}  // namespace collector::sources
