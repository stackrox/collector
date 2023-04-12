
#ifndef COLLECTOR_SELF_CHECK_HANDLE_H
#define COLLECTOR_SELF_CHECK_HANDLE_H

#include "SelfChecks.h"
#include "SignalHandler.h"
#include "SysdigEventExtractor.h"

namespace collector {

class SelfCheckHandler {
 public:
  SelfCheckHandler(sinsp* inspector) : inspector_(inspector) {
    event_extractor_.Init(inspector);
  }

  bool isSelfCheckEvent(sinsp_evt* evt) {
    const std::string* name = event_extractor_.get_comm(evt);
    const std::string* exe = event_extractor_.get_exe(evt);

    if (name == nullptr || exe == nullptr) {
      return false;
    }

    return name->compare(self_checks::kSelfChecksName) == 0 && exe->compare(self_checks::kSelfChecksExePath);
  }

 protected:
  sinsp* inspector_;
  SysdigEventExtractor event_extractor_;
};

class SelfCheckProcessHandler : public SelfCheckHandler, public SignalHandler {
 public:
  SelfCheckProcessHandler(sinsp* inspector) : SelfCheckHandler(inspector),
                                              found_live_event_(false),
                                              found_collector_process_(false) {
  }

  std::string GetName() override {
    return "SelfCheckProcessHandler";
  }

  std::vector<std::string> GetRelevantEvents() {
    return {"execve<"};
  }

  Result HandleSignal(sinsp_evt* evt) override;
  Result HandleExistingProcess(sinsp_threadinfo* tinfo) override;

 private:
  bool found_live_event_;
  bool found_collector_process_;

  bool Finished() const {
    return found_live_event_ && found_collector_process_;
  }
};

class SelfCheckNetworkHandler : public SelfCheckHandler, public SignalHandler {
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
