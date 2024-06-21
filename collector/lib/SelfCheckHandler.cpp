#include "SelfCheckHandler.h"

#include <array>
#include <chrono>

#include "Logging.h"
#include "SelfChecks.h"
#include "system-inspector/EventExtractor.h"

namespace collector {

SelfCheckHandler::SelfCheckHandler(
    sinsp* inspector,
    std::chrono::seconds timeout) : inspector_(inspector), event_extractor_(std::make_unique<system_inspector::EventExtractor>()), timeout_(timeout) {
  event_extractor_->Init(inspector);
  start_ = std::chrono::steady_clock::now();
}

bool SelfCheckHandler::isSelfCheckEvent(sinsp_evt* evt) {
  const std::string* name = event_extractor_->get_comm(evt);
  const std::string* exe = event_extractor_->get_exe(evt);

  if (name == nullptr || exe == nullptr) {
    return false;
  }

  return name->compare(self_checks::kSelfChecksName) == 0 && exe->compare(self_checks::kSelfChecksExePath) == 0;
}

bool SelfCheckHandler::hasTimedOut() {
  auto now = std::chrono::steady_clock::now();
  return now > (start_ + timeout_);
}

SignalHandlerResult SelfCheckProcessHandler::HandleSignal(sinsp_evt* evt) {
  if (hasTimedOut()) {
    CLOG(WARNING) << "Failed to detect any self-check process events within the timeout.";
    return {std::nullopt, FINISHED};
  }

  if (isSelfCheckEvent(evt)) {
    CLOG(INFO) << "Found self-check process event.";
    return {std::nullopt, FINISHED};
  }

  return {std::nullopt, IGNORED};
}

SignalHandlerResult SelfCheckNetworkHandler::HandleSignal(sinsp_evt* evt) {
  if (hasTimedOut()) {
    CLOG(WARNING) << "Failed to detect any self-check networking events within the timeout.";
    return {std::nullopt, IGNORED};
  }

  if (!isSelfCheckEvent(evt)) {
    return {std::nullopt, IGNORED};
  }

  const uint16_t* server_port = event_extractor_->get_server_port(evt);
  const uint16_t* client_port = event_extractor_->get_client_port(evt);

  if (server_port == nullptr || client_port == nullptr) {
    return {std::nullopt, IGNORED};
  }

  if (*server_port == self_checks::kSelfCheckServerPort && *client_port != (uint16_t)-1) {
    CLOG(INFO) << "Found self-check connection event.";
    return {std::nullopt, FINISHED};
  }

  return {std::nullopt, IGNORED};
}

}  // namespace collector
