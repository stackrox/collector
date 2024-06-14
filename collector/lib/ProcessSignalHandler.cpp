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
  client_->Start();
  return true;
}

bool ProcessSignalHandler::Stop() {
  client_->Stop();
  rate_limiter_.ResetRateLimitCache();
  return true;
}

SignalHandlerResult ProcessSignalHandler::HandleSignal(sinsp_evt* evt) {
  const auto* signal_msg = formatter_.ToProtoMessage(evt);

  if (!signal_msg) {
    ++(stats_->nProcessResolutionFailuresByEvt);
    return {std::nullopt, IGNORED};
  }

  const char* name = signal_msg->signal().process_signal().name().c_str();
  const int pid = signal_msg->signal().process_signal().pid();
  DTRACE_PROBE2(collector, process_signal_handler, name, pid);

  if (!rate_limiter_.Allow(compute_process_key(signal_msg->signal().process_signal()))) {
    ++(stats_->nProcessRateLimitCount);
    return {std::nullopt, IGNORED};
  }

  return {*signal_msg, PROCESSED};
}

SignalHandlerResult ProcessSignalHandler::HandleExistingProcess(sinsp_threadinfo* tinfo) {
  const auto* signal_msg = formatter_.ToProtoMessage(tinfo);
  if (!signal_msg) {
    ++(stats_->nProcessResolutionFailuresByTinfo);
    return {std::nullopt, IGNORED};
  }

  if (!rate_limiter_.Allow(compute_process_key(signal_msg->signal().process_signal()))) {
    ++(stats_->nProcessRateLimitCount);
    return {std::nullopt, IGNORED};
  }

  return {*signal_msg, PROCESSED};
}

std::vector<std::string> ProcessSignalHandler::GetRelevantEvents() {
  return {"execve<"};
}

}  // namespace collector
