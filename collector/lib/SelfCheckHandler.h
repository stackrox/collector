
#ifndef COLLECTOR_SELF_CHECK_HANDLE_H
#define COLLECTOR_SELF_CHECK_HANDLE_H

#include <chrono>

#include "SelfChecks.h"
#include "SignalHandler.h"
#include "SysdigEventExtractor.h"

namespace collector {

class SelfCheckHandler : public SignalHandler {
 public:
  SelfCheckHandler() {}
  SelfCheckHandler(
      sinsp* inspector,
      std::chrono::seconds timeout = std::chrono::seconds(5)) : inspector_(inspector), timeout_(timeout) {
    event_extractor_.Init(inspector);
    start_ = std::chrono::steady_clock::now();
  }

 protected:
  sinsp* inspector_;
  SysdigEventExtractor event_extractor_;

  std::chrono::time_point<std::chrono::steady_clock> start_;
  std::chrono::seconds timeout_;

  bool isSelfCheckEvent(sinsp_evt* evt) {
    const std::string* name = event_extractor_.get_comm(evt);
    const std::string* exe = event_extractor_.get_exe(evt);

    if (name == nullptr || exe == nullptr) {
      return false;
    }

    return name->compare(self_checks::kSelfChecksName) == 0 && exe->compare(self_checks::kSelfChecksExePath) == 0;
  }

  bool hasTimedOut() {
    auto now = std::chrono::steady_clock::now();
    return now > (start_ + timeout_);
  }
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

  virtual Result HandleSignal(sinsp_evt* evt) override;
  virtual Result HandleExistingProcess(sinsp_threadinfo* tinfo) override;
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

  Result HandleSignal(sinsp_evt* evt) override;
  Result HandleExistingProcess(sinsp_threadinfo* tinfo) override;
};

}  // namespace collector

#endif
