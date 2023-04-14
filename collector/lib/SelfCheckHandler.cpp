#include "SelfCheckHandler.h"

#include <array>
#include <chrono>

#include "Logging.h"
#include "SelfChecks.h"

namespace collector {

SignalHandler::Result SelfCheckProcessHandler::HandleSignal(sinsp_evt* evt) {
  if (!isSelfCheckEvent(evt)) {
    CLOG(INFO) << "Found self-check process event.";
    return FINISHED;
  }

  // check for timeout second, in case this event is from the self-check
  // to avoid false negative.
  if (hasTimedOut()) {
    CLOG(WARNING) << "Failed to detect any self-check process events.";
    return FINISHED;
  }

  return IGNORED;
}

SignalHandler::Result SelfCheckProcessHandler::HandleExistingProcess(sinsp_threadinfo* tinfo) {
  return IGNORED;
}

SignalHandler::Result SelfCheckNetworkHandler::HandleSignal(sinsp_evt* evt) {
  if (!isSelfCheckEvent(evt)) {
    return IGNORED;
  }

  const uint16_t* server_port = event_extractor_.get_server_port(evt);
  const uint16_t* client_port = event_extractor_.get_client_port(evt);

  if (server_port == nullptr || client_port == nullptr) {
    return IGNORED;
  }

  if (*server_port == 1337 && *client_port != (uint16_t)-1) {
    CLOG(INFO) << "Found self-check connection event.";
    return FINISHED;
  }

  // check for timeout last, in case this event is from the self-check
  // to avoid false negative.
  if (hasTimedOut()) {
    CLOG(WARNING) << "Failed to detect any self-check networking events.";
    return FINISHED;
  }

  return IGNORED;
}

SignalHandler::Result SelfCheckNetworkHandler::HandleExistingProcess(sinsp_threadinfo* tinfo) {
  return IGNORED;
}

}  // namespace collector
