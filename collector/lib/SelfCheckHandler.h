
#ifndef COLLECTOR_SELF_CHECK_HANDLE_H
#define COLLECTOR_SELF_CHECK_HANDLE_H

#include <chrono>
#include <memory>

#include "SelfChecks.h"
#include "SignalHandler.h"

// forward declaration
class sinsp;
namespace collector {
namespace system_inspector {
class EventExtractor;
}
}  // namespace collector

namespace collector {

class SelfCheckHandler : public SignalHandler {
 public:
  SelfCheckHandler() {}
  SelfCheckHandler(
      sinsp* inspector,
      std::chrono::seconds timeout = std::chrono::seconds(5));

 protected:
  sinsp* inspector_;
  std::unique_ptr<system_inspector::EventExtractor> event_extractor_;

  std::chrono::time_point<std::chrono::steady_clock> start_;
  std::chrono::seconds timeout_;

  /**
   * @brief Verifies that a given event came from the self-check process,
   * by checking the process name and the executable path.
   *
   * @note pid verification is not possible because the driver retrieves
   *       the host pid, but when we fork the process we get the namespace
   *       pid.
   */
  bool isSelfCheckEvent(sinsp_evt* evt);

  /**
   * @brief simple check that the handler has timed out waiting for
   *        self check events.
   */
  bool hasTimedOut();
};

class SelfCheckProcessHandler : public SelfCheckHandler {
 public:
  SelfCheckProcessHandler(sinsp* inspector) : SelfCheckHandler(inspector) {
  }

  std::string GetName() override {
    return "SelfCheckProcessHandler";
  }

  std::vector<std::string> GetRelevantEvents() {
    return {"execve<"};
  }

  virtual SignalHandlerResult HandleSignal(sinsp_evt* evt) override;
};

class SelfCheckNetworkHandler : public SelfCheckHandler {
 public:
  SelfCheckNetworkHandler(sinsp* inspector) : SelfCheckHandler(inspector) {
  }

  std::string GetName() override {
    return "SelfCheckNetworkHandler";
  }

  std::vector<std::string> GetRelevantEvents() {
    return {
        "close<",
        "shutdown<",
        "connect<",
        "accept<",
        "listen<",
    };
  }

  SignalHandlerResult HandleSignal(sinsp_evt* evt) override;
};

}  // namespace collector

#endif
