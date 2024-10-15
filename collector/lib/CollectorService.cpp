#include "CollectorService.h"

#include "CollectionMethod.h"
#include "ContainerInfoInspector.h"

extern "C" {
#include <signal.h>
}

#include <memory>

#include "CivetServer.h"
#include "CollectorConfigInspector.h"
#include "CollectorStatsExporter.h"
#include "ConnTracker.h"
#include "Containers.h"
#include "Diagnostics.h"
#include "GRPCUtil.h"
#include "GetStatus.h"
#include "LogLevel.h"
#include "NetworkStatusInspector.h"
#include "NetworkStatusNotifier.h"
#include "ProfilerHandler.h"
#include "Utility.h"
#include "prometheus/exposer.h"
#include "system-inspector/Service.h"

extern unsigned char g_bpf_drop_syscalls[];  // defined in libscap

namespace collector {

CollectorService::CollectorService(CollectorConfig& config, std::atomic<ControlValue>* control,
                                   const std::atomic<int>* signum)
    : config_(config), control_(control), signum_(*signum) {
  CLOG(INFO) << "Config: " << config;
}

void CollectorService::RunForever() {
  // Start monitoring services.
  // Some of these variables must remain in scope, so
  // be cautious if decomposing to a separate function.
  const char* options[] = {"listening_ports", "8080", 0};
  CivetServer server(options);

  std::shared_ptr<ConnectionTracker> conn_tracker;

  GetStatus getStatus(config_.Hostname(), &system_inspector_);

  std::shared_ptr<prometheus::Registry> registry = std::make_shared<prometheus::Registry>();

  server.addHandler("/ready", getStatus);
  LogLevelHandler setLogLevel;
  server.addHandler("/loglevel", setLogLevel);

  ProfilerHandler profiler_handler;
  server.addHandler(ProfilerHandler::kBaseRoute, profiler_handler);

  prometheus::Exposer exposer("9090");
  exposer.RegisterCollectable(registry);

  CollectorStatsExporter exporter(registry, &config_, &system_inspector_);

  std::unique_ptr<NetworkStatusNotifier> net_status_notifier;

  std::unique_ptr<ContainerInfoInspector> container_info_inspector;
  std::unique_ptr<NetworkStatusInspector> network_status_inspector;
  std::unique_ptr<CollectorConfigInspector> collector_config_inspector;

  CLOG(INFO) << "Network scrape interval set to " << config_.ScrapeInterval() << " seconds";

  if (config_.grpc_channel) {
    CLOG(INFO) << "Waiting for Sensor to become ready ...";
    if (!WaitForGRPCServer()) {
      CLOG(INFO) << "Interrupted while waiting for Sensor to become ready ...";
      return;
    }
    CLOG(INFO) << "Sensor connectivity is successful";
  }

  if (!config_.grpc_channel || !config_.DisableNetworkFlows()) {
    // In case if no GRPC is used, continue to setup networking infrasturcture
    // with empty grpc_channel. NetworkConnectionInfoServiceComm will pick it
    // up and use stdout instead.
    std::shared_ptr<ProcessStore> process_store;
    if (config_.IsProcessesListeningOnPortsEnabled()) {
      process_store = std::make_shared<ProcessStore>(&system_inspector_);
    }
    std::shared_ptr<IConnScraper> conn_scraper = std::make_shared<ConnScraper>(config_.HostProc(), process_store);
    conn_tracker = std::make_shared<ConnectionTracker>();
    UnorderedSet<L4ProtoPortPair> ignored_l4proto_port_pairs(config_.IgnoredL4ProtoPortPairs());
    conn_tracker->UpdateIgnoredL4ProtoPortPairs(std::move(ignored_l4proto_port_pairs));
    conn_tracker->UpdateIgnoredNetworks(config_.IgnoredNetworks());
    conn_tracker->UpdateNonAggregatedNetworks(config_.NonAggregatedNetworks());

    auto network_connection_info_service_comm = std::make_shared<NetworkConnectionInfoServiceComm>(config_.Hostname(), config_.grpc_channel);

    net_status_notifier = MakeUnique<NetworkStatusNotifier>(conn_scraper,
                                                            conn_tracker,
                                                            network_connection_info_service_comm,
                                                            config_,
                                                            config_.EnableConnectionStats() ? exporter.GetConnectionsTotalReporter() : 0,
                                                            config_.EnableConnectionStats() ? exporter.GetConnectionsRateReporter() : 0);
    net_status_notifier->Start();
  }

  if (!exporter.start()) {
    CLOG(FATAL) << "Unable to start collector stats exporter";
  }

  if (config_.IsIntrospectionEnabled()) {
    container_info_inspector = std::make_unique<ContainerInfoInspector>(system_inspector_.GetContainerMetadataInspector());
    server.addHandler(container_info_inspector->kBaseRoute, container_info_inspector.get());
    network_status_inspector = std::make_unique<NetworkStatusInspector>(conn_tracker);
    server.addHandler(network_status_inspector->kBaseRoute, network_status_inspector.get());
    collector_config_inspector = std::make_unique<CollectorConfigInspector>(config_);
    server.addHandler(collector_config_inspector->kBaseRoute, collector_config_inspector.get());
  }

  system_inspector_.Init(config_, conn_tracker);
  system_inspector_.Start();

  ControlValue cv;
  while ((cv = control_->load(std::memory_order_relaxed)) != STOP_COLLECTOR) {
    system_inspector_.Run(*control_);
    CLOG(DEBUG) << "Interrupted collector!";
  }

  int signal = signum_.load();

  if (signal != 0) {
    CLOG(INFO) << "Caught signal " << signal << " (" << SignalName(signal) << "): " << strsignal(signal);
  }

  CLOG(INFO) << "Shutting down collector.";

  if (net_status_notifier) {
    net_status_notifier->Stop();
  }
  // Shut down these first since they access the system inspector object.
  exporter.stop();
  server.close();

  system_inspector_.CleanUp();
}

bool CollectorService::InitKernel() {
  return system_inspector_.InitKernel(config_);
}

bool CollectorService::WaitForGRPCServer() {
  std::string error_str;
  auto interrupt = [this] { return control_->load(std::memory_order_relaxed) == STOP_COLLECTOR; };
  return WaitForChannelReady(config_.grpc_channel, interrupt);
}

bool SetupKernelDriver(CollectorService& collector, const CollectorConfig& config) {
  auto& startup_diagnostics = StartupDiagnostics::GetInstance();
  std::string cm_name(CollectionMethodName(config.GetCollectionMethod()));

  startup_diagnostics.DriverAvailable(cm_name);

  if (collector.InitKernel()) {
    startup_diagnostics.DriverSuccess(cm_name);
    return true;
  }

  CLOG(ERROR) << "Failed to initialize collector kernel components.";
  // No candidate managed to create a working collector service.
  return false;
}

}  // namespace collector
