#include "ProcessSignalHandler.h"

#include <optional>
#include <sstream>

#include <sys/sdt.h>

#include <libsinsp/sinsp.h>

#include "storage/process_indicator.pb.h"

#include "RateLimit.h"
#include "system-inspector/EventExtractor.h"

namespace collector {

std::string compute_process_key(const ::storage::ProcessSignal& s) {
  std::stringstream ss;
  ss << s.container_id() << " " << s.name() << " ";
  if (s.args().length() <= 256) {
    ss << s.args();
  } else {
    ss.write(s.args().c_str(), 256);
  }
  ss << " " << s.exec_file_path();
  return ss.str();
}

bool ProcessSignalHandler::Start() {
  return true;
}

bool ProcessSignalHandler::Stop() {
  rate_limiter_.ResetRateLimitCache();
  return true;
}

SignalHandler::Result ProcessSignalHandler::HandleSignal(sinsp_evt* evt) {
  const auto* signal_msg = formatter_.ToProtoMessage(evt);

  if (!signal_msg) {
    ++(stats_->nProcessResolutionFailuresByEvt);
    return {std::nullopt, SignalHandler::IGNORED};
  }

  const char* name = signal_msg->signal().process_signal().name().c_str();
  const int pid = signal_msg->signal().process_signal().pid();
  DTRACE_PROBE2(collector, process_signal_handler, name, pid);

  if (!rate_limiter_.Allow(compute_process_key(signal_msg->signal().process_signal()))) {
    ++(stats_->nProcessRateLimitCount);
    return {std::nullopt, SignalHandler::IGNORED};
  }

  return {*signal_msg, SignalHandler::PROCESSED};
}

SignalHandler::Result ProcessSignalHandler::HandleExistingProcess(sinsp_threadinfo* tinfo) {
  const auto* signal_msg = formatter_.ToProtoMessage(tinfo);
  if (!signal_msg) {
    ++(stats_->nProcessResolutionFailuresByTinfo);
    return {std::nullopt, SignalHandler::IGNORED};
  }

  if (!rate_limiter_.Allow(compute_process_key(signal_msg->signal().process_signal()))) {
    ++(stats_->nProcessRateLimitCount);
    return {std::nullopt, SignalHandler::IGNORED};
  }

  return {*signal_msg, SignalHandler::PROCESSED};
}

std::vector<std::string> ProcessSignalHandler::GetRelevantEvents() {
  return {"execve<"};
}

}  // namespace collector
