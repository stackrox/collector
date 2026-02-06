#pragma once

#include <prometheus/exposer.h>
#include <prometheus/registry.h>

#include "CivetWrapper.h"
#include "CollectorConfig.h"
#include "CollectorStatsExporter.h"
#include "ConfigLoader.h"
#include "Control.h"
#include "NetworkStatusInspector.h"
#include "NetworkStatusNotifier.h"
#include "events/Dispatcher.h"
#include "system-inspector/Service.h"

namespace collector {

class CollectorService {
 public:
  CollectorService(const CollectorService&) = delete;
  CollectorService(CollectorService&&) = delete;
  CollectorService& operator=(const CollectorService&) = delete;
  CollectorService& operator=(CollectorService&&) = delete;
  ~CollectorService();

  CollectorService(CollectorConfig& config, std::atomic<ControlValue>* control, const std::atomic<int>* signum);

  void RunForever();

  bool InitKernel();

 private:
  bool WaitForGRPCServer();

  collector::events::EventDispatcher event_dispatcher_;

  CollectorConfig& config_;
  system_inspector::Service system_inspector_;

  std::atomic<ControlValue>* control_;
  const std::atomic<int>& signum_;

  // Civetweb handlers and server
  CivetServer server_;
  std::vector<std::unique_ptr<CivetWrapper>> civet_endpoints_;

  // Prometheus
  prometheus::Exposer exposer_;
  CollectorStatsExporter exporter_;

  ConfigLoader config_loader_;

  // Network monitoring
  std::shared_ptr<ConnectionTracker> conn_tracker_;
  std::unique_ptr<NetworkStatusNotifier> net_status_notifier_;
  std::shared_ptr<ProcessStore> process_store_;
  std::shared_ptr<NetworkConnectionInfoServiceComm> network_connection_info_service_comm_;
};

}  // namespace collector
