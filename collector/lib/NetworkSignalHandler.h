#pragma once

#include <memory>
#include <optional>

#include "ConnTracker.h"
#include "SignalHandler.h"
#include "system-inspector/SystemInspector.h"

// forward declarations
class sinsp;
class sinsp_evt;

namespace collector {
namespace system_inspector {
class EventExtractor;
}

class NetworkSignalHandler final : public SignalHandler {
 public:
  explicit NetworkSignalHandler(sinsp* inspector, std::shared_ptr<ConnectionTracker> conn_tracker, system_inspector::Stats* stats);
  ~NetworkSignalHandler() override;

  std::string GetName() override { return "NetworkSignalHandler"; }
  Result HandleSignal(sinsp_evt* evt) override;
  std::vector<std::string> GetRelevantEvents() override;
  bool Stop() override;

  void SetCollectConnectionStatus(bool collect_connection_status) { collect_connection_status_ = collect_connection_status; }
  void SetTrackSendRecv(bool track_send_recv) { track_send_recv_ = track_send_recv; }

 private:
  std::optional<Connection> GetConnection(sinsp_evt* evt);

  std::unique_ptr<system_inspector::EventExtractor> event_extractor_;
  std::shared_ptr<ConnectionTracker> conn_tracker_;
  system_inspector::Stats* stats_;

  bool collect_connection_status_;
  bool track_send_recv_;
};

}  // namespace collector
