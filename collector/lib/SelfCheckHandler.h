#pragma once

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

/// Base for startup self-check handlers. Collector forks a known binary
/// and these handlers wait for the BPF driver to deliver matching events,
/// proving the probe loaded and the event pipeline works end-to-end.
/// Returns FINISHED on match or timeout to remove itself from the pipeline.
///
/// Matches on process name + exe path rather than PID because the driver
/// reports host-namespace PIDs while the fork gives container-namespace PIDs.
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

/// Verifies BPF process-event delivery by watching for execve events
/// from the self-check binary.
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

  virtual Result HandleSignal(sinsp_evt* evt) override;
};

/// Verifies BPF network-event delivery by watching for connection
/// lifecycle events from the self-check binary on the expected port.
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

  Result HandleSignal(sinsp_evt* evt) override;
};

}  // namespace collector
