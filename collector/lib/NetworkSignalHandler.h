#ifndef COLLECTOR_NETWORKSIGNALHANDLER_H
#define COLLECTOR_NETWORKSIGNALHANDLER_H

#include "ConnTracker.h"
#include "SignalHandler.h"
#include "SysdigEventExtractor.h"
#include "SysdigService.h"

namespace collector {

class NetworkSignalHandler final : public SignalHandler {
 public:
  explicit NetworkSignalHandler(sinsp* inspector, std::shared_ptr<ConnectionTracker> conn_tracker, SysdigStats* stats)
      : conn_tracker_(std::move(conn_tracker)), stats_(stats) {
    event_extractor_.Init(inspector);
  }

  std::string GetName() override { return "NetworkSignalHandler"; }
  Result HandleSignal(sinsp_evt* evt) override;
  std::vector<std::string> GetRelevantEvents() override;
  bool Stop() override;

 private:
  std::pair<Connection, bool> GetConnection(sinsp_evt* evt);

  SysdigEventExtractor event_extractor_;
  std::shared_ptr<ConnectionTracker> conn_tracker_;
  SysdigStats* stats_;
};

}  // namespace collector

#endif  //COLLECTOR_NETWORKSIGNALHANDLER_H
