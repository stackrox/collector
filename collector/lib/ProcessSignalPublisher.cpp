#include "ProcessSignalPublisher.h"

#include <sstream>

#include "storage/process_indicator.pb.h"

namespace collector {

std::string ProcessSignalPublisher::ProcessKey(const storage::ProcessSignal& signal) {
  std::stringstream key;
  key << signal.container_id() << " " << signal.name() << " ";
  if (signal.args().length() <= 256) {
    key << signal.args();
  } else {
    key.write(signal.args().c_str(), 256);
  }
  key << " " << signal.exec_file_path();
  return key.str();
}

SignalHandler::Result ProcessSignalPublisher::Publish(
    const sensor::SignalStreamMessage& signal) {
  const auto& process_signal = signal.signal().process_signal();
  if (!rate_limiter_.Allow(ProcessKey(process_signal))) {
    ++stats_->nProcessRateLimitCount;
    return SignalHandler::IGNORED;
  }

  const auto result = client_->PushSignals(signal);
  RecordResult(result);
  return result;
}

void ProcessSignalPublisher::RecordResult(SignalHandler::Result result) {
  if (result == SignalHandler::PROCESSED) {
    ++stats_->nProcessSent;
  } else if (result == SignalHandler::ERROR) {
    ++stats_->nProcessSendFailures;
  }
}

}  // namespace collector
