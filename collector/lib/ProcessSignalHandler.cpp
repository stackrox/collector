#include "ProcessSignalHandler.h"

#include <sstream>

#include <sys/sdt.h>

#include <libsinsp/sinsp.h>

#include "RateLimit.h"

namespace collector {

namespace {
// The template functions in this namespace are meant to be used with
// sensor::ProcessSignal and storage::ProcessSignal, which are almost
// the same... Except they are not...
template <typename S>
std::string compute_process_key(const S& s) {
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

template <typename S>
void dtrace_probe(const S& s) {
  const char* name = s.name().c_str();
  const uint32_t pid = s.pid();
  DTRACE_PROBE2(collector, process_signal_handler, name, pid);
}
}  // namespace

SignalHandler::Result ProcessSignalHandler::HandleSignal(sinsp_evt* evt) {
  if (client_->UseSensorClient()) {
    return HandleSensorSignal(evt);
  }
  return HandleProcessSignal(evt);
}

SignalHandler::Result ProcessSignalHandler::HandleExistingProcess(sinsp_threadinfo* tinfo) {
  if (client_->UseSensorClient()) {
    return HandleExistingProcessSensor(tinfo);
  }
  return HandleExistingProcessSignal(tinfo);
}

SignalHandler::Result ProcessSignalHandler::HandleProcessSignal(sinsp_evt* evt) {
  const auto* signal_msg = signal_formatter_.ToProtoMessage(evt);

  if (signal_msg == nullptr) {
    ++(stats_->nProcessResolutionFailuresByEvt);
    return IGNORED;
  }

  dtrace_probe(signal_msg->signal().process_signal());

  if (!rate_limiter_.Allow(compute_process_key(signal_msg->signal().process_signal()))) {
    ++(stats_->nProcessRateLimitCount);
    return IGNORED;
  }

  auto result = client_->SendMsg(*signal_msg);
  if (result == SignalHandler::PROCESSED) {
    ++(stats_->nProcessSent);
  } else if (result == SignalHandler::ERROR) {
    ++(stats_->nProcessSendFailures);
  }

  return result;
}

SignalHandler::Result ProcessSignalHandler::HandleExistingProcessSignal(sinsp_threadinfo* tinfo) {
  const auto* signal_msg = signal_formatter_.ToProtoMessage(tinfo);
  if (signal_msg == nullptr) {
    ++(stats_->nProcessResolutionFailuresByTinfo);
    return IGNORED;
  }

  if (!rate_limiter_.Allow(compute_process_key(signal_msg->signal().process_signal()))) {
    ++(stats_->nProcessRateLimitCount);
    return IGNORED;
  }

  auto result = client_->SendMsg(*signal_msg);
  if (result == SignalHandler::PROCESSED) {
    ++(stats_->nProcessSent);
  } else if (result == SignalHandler::ERROR) {
    ++(stats_->nProcessSendFailures);
  }

  return result;
}

SignalHandler::Result ProcessSignalHandler::HandleSensorSignal(sinsp_evt* evt) {
  const auto* signal_msg = sensor_formatter_.ToProtoMessage(evt);

  if (signal_msg == nullptr) {
    ++(stats_->nProcessResolutionFailuresByEvt);
    return IGNORED;
  }

  dtrace_probe(signal_msg->process_signal());

  if (!rate_limiter_.Allow(compute_process_key(signal_msg->process_signal()))) {
    ++(stats_->nProcessRateLimitCount);
    return IGNORED;
  }

  auto result = client_->SendMsg(*signal_msg);
  if (result == SignalHandler::PROCESSED) {
    ++(stats_->nProcessSent);
  } else if (result == SignalHandler::ERROR) {
    ++(stats_->nProcessSendFailures);
  }

  return result;
}

SignalHandler::Result ProcessSignalHandler::HandleExistingProcessSensor(sinsp_threadinfo* tinfo) {
  const auto* signal_msg = sensor_formatter_.ToProtoMessage(tinfo);
  if (signal_msg == nullptr) {
    ++(stats_->nProcessResolutionFailuresByTinfo);
    return IGNORED;
  }

  if (!rate_limiter_.Allow(compute_process_key(signal_msg->process_signal()))) {
    ++(stats_->nProcessRateLimitCount);
    return IGNORED;
  }

  auto result = client_->SendMsg(*signal_msg);
  if (result == SignalHandler::PROCESSED) {
    ++(stats_->nProcessSent);
  } else if (result == SignalHandler::ERROR) {
    ++(stats_->nProcessSendFailures);
  }

  return result;
}
std::vector<std::string> ProcessSignalHandler::GetRelevantEvents() {
  return {"execve<"};
}

}  // namespace collector
