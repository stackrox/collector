#include "ProcessSignalHandler.h"

#include <sys/sdt.h>

#include <libsinsp/sinsp.h>

#include "system-inspector/EventExtractor.h"

namespace collector {

bool ProcessSignalHandler::Start() {
  client_->Start();
  return true;
}

bool ProcessSignalHandler::Stop() {
  client_->Stop();
  publisher_.Reset();
  return true;
}

SignalHandler::Result ProcessSignalHandler::HandleSignal(sinsp_evt* evt) {
  const auto* signal_msg = formatter_.ToProtoMessage(evt);

  if (!signal_msg) {
    ++(stats_->nProcessResolutionFailuresByEvt);
    return IGNORED;
  }

  const char* name = signal_msg->signal().process_signal().name().c_str();
  const int pid = signal_msg->signal().process_signal().pid();
  DTRACE_PROBE2(collector, process_signal_handler, name, pid);

  return publisher_.Publish(*signal_msg);
}

SignalHandler::Result ProcessSignalHandler::HandleExistingProcess(sinsp_threadinfo* tinfo) {
  const auto* signal_msg = formatter_.ToProtoMessage(tinfo);
  if (!signal_msg) {
    ++(stats_->nProcessResolutionFailuresByTinfo);
    return IGNORED;
  }

  return publisher_.Publish(*signal_msg);
}

std::vector<std::string> ProcessSignalHandler::GetRelevantEvents() {
  return {"execve<"};
}

}  // namespace collector
