#include "SelfCheckHandler.h"

#include <array>
#include <chrono>

#include "Logging.h"
#include "SelfChecks.h"

namespace collector {

SignalHandler::Result SelfCheckProcessHandler::HandleSignal(sinsp_evt* evt) {
  if (hasTimedOut()) {
    CLOG(WARNING) << "Failed to detect any self-check process events within the timeout.";
    return FINISHED;
  }

  if (isSelfCheckEvent(evt)) {
    CLOG(INFO) << "Found self-check process event.";
    return FINISHED;
  }

  return IGNORED;
}

SignalHandler::Result SelfCheckNetworkHandler::HandleSignal(sinsp_evt* evt) {
  if (hasTimedOut()) {
    CLOG(WARNING) << "Failed to detect any self-check networking events within the timeout.";
    return FINISHED;
  }

  if (!isSelfCheckEvent(evt)) {
    return IGNORED;
  }

  const uint16_t* server_port = event_extractor_.get_server_port(evt);
  const uint16_t* client_port = event_extractor_.get_client_port(evt);

  if (server_port == nullptr || client_port == nullptr) {
    return IGNORED;
  }

  if (*server_port == self_checks::kSelfCheckServerPort && *client_port != (uint16_t)-1) {
    CLOG(INFO) << "Found self-check connection event.";
    return FINISHED;
  }

  return IGNORED;
}

}  // namespace collector
