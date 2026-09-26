#pragma once

#include <string>

#include "storage/process_indicator.pb.h"

#include "RateLimit.h"
#include "SignalHandler.h"
#include "SignalServiceClient.h"
#include "system-inspector/SystemInspector.h"

namespace collector {

// Owns the common delivery policy for process signals. Formatting and event
// resolution remain in ProcessSignalHandler; this class only decides whether
// a formatted signal may be sent and records the delivery outcome.
class ProcessSignalPublisher {
 public:
  ProcessSignalPublisher(ISignalServiceClient* client, system_inspector::Stats* stats)
      : client_(client), stats_(stats) {}

  SignalHandler::Result Publish(const sensor::SignalStreamMessage& signal);
  void Reset() { rate_limiter_.ResetRateLimitCache(); }

 private:
  static std::string ProcessKey(const storage::ProcessSignal& signal);
  void RecordResult(SignalHandler::Result result);

  ISignalServiceClient* client_;
  system_inspector::Stats* stats_;
  RateLimitCache rate_limiter_;
};

}  // namespace collector
