#include "CollectorService.h"

#include <memory>

#include "CollectionMethod.h"
#include "CollectorRuntimeConfigInspector.h"
#include "CollectorStatsExporter.h"
#include "ConfigLoader.h"
#include "ConnTracker.h"
#include "ContainerInfoInspector.h"
#include "Diagnostics.h"
#include "GRPCUtil.h"
#include "GetStatus.h"
#include "LogLevel.h"
#include "NetworkSignalHandler.h"
#include "NetworkStatusInspector.h"
#include "NetworkStatusNotifier.h"
#include "ProfilerHandler.h"
#include "Utility.h"
#include "system-inspector/Service.h"

namespace collector {

static const char* OPTIONS[] = {"listening_ports", "8080", nullptr};
static const std::string PROMETHEUS_PORT = "9090";

CollectorService::CollectorService(CollectorConfig& config, std::atomic<ControlValue>* control,
                                   const std::atomic<int>* signum)
    : config_(config),
      system_inspector_(config_, event_dispatcher_),
      control_(control),
      signum_(*signum),
      server_(OPTIONS),
      exposer_(PROMETHEUS_PORT),
      exporter_(&config_, &system_inspector_),
      config_loader_(config_) {
  CLOG(INFO) << "Config: " << config_;

  // Network tracking
  if (!config_.grpc_channel || !config_.DisableNetworkFlows()) {
    // In case if no GRPC is used, continue to setup networking infrasturcture
    // with empty grpc_channel. NetworkConnectionInfoServiceComm will pick it
    // up and use stdout instead.
    conn_tracker_ = std::make_shared<ConnectionTracker>();
    UnorderedSet<L4ProtoPortPair> ignored_l4proto_port_pairs(config_.IgnoredL4ProtoPortPairs());
    conn_tracker_->UpdateIgnoredL4ProtoPortPairs(std::move(ignored_l4proto_port_pairs));
    conn_tracker_->UpdateIgnoredNetworks(config_.IgnoredNetworks());
    conn_tracker_->UpdateNonAggregatedNetworks(config_.NonAggregatedNetworks());

    net_status_notifier_ = std::make_unique<NetworkStatusNotifier>(
        conn_tracker_,
        config_,
        &system_inspector_,
        exporter_.GetRegistry().get());

    auto network_signal_handler = std::make_unique<NetworkSignalHandler>(system_inspector_.GetInspector(), conn_tracker_, system_inspector_.GetUserspaceStats());
    network_signal_handler->SetCollectConnectionStatus(config_.CollectConnectionStatus());
    network_signal_handler->SetTrackSendRecv(config_.TrackingSendRecv());
    system_inspector_.AddSignalHandler(std::move(network_signal_handler));
  }

  // Initialize civetweb server handlers
  civet_endpoints_.emplace_back(std::make_unique<GetStatus>(&system_inspector_));
  civet_endpoints_.emplace_back(std::make_unique<LogLevelHandler>());
  civet_endpoints_.emplace_back(std::make_unique<ProfilerHandler>());

  if (config.IsIntrospectionEnabled()) {
    civet_endpoints_.emplace_back(std::make_unique<ContainerInfoInspector>(system_inspector_.GetContainerMetadataInspector()));
    civet_endpoints_.emplace_back(std::make_unique<NetworkStatusInspector>(conn_tracker_));
    civet_endpoints_.emplace_back(std::make_unique<CollectorConfigInspector>(config_));
  }

  for (const auto& endpoint : civet_endpoints_) {
    server_.addHandler(endpoint->GetBaseRoute(), endpoint.get());
  }

  // Prometheus
  exposer_.RegisterCollectable(exporter_.GetRegistry());
}

CollectorService::~CollectorService() {
  config_loader_.Stop();
  server_.close();
  if (net_status_notifier_) {
    net_status_notifier_->Stop();
  }

  exporter_.stop();

  // system_inspector_ needs to be last, since other components relay on it.
  system_inspector_.CleanUp();
}

void CollectorService::RunForever() {
  // Start monitoring services.
  config_loader_.Start();

  CLOG(INFO) << "Network scrape interval set to " << config_.ScrapeInterval() << " seconds";

  if (net_status_notifier_) {
    net_status_notifier_->Start();
  }

  if (!exporter_.start()) {
    CLOG(FATAL) << "Unable to start collector stats exporter";
  }

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
}

bool CollectorService::WaitForGRPCServer() {
  std::string error_str;
  auto interrupt = [this] { return control_->load(std::memory_order_relaxed) == STOP_COLLECTOR; };
  return WaitForChannelReady(config_.grpc_channel, interrupt);
}

bool CollectorService::InitKernel() {
  auto& startup_diagnostics = StartupDiagnostics::GetInstance();
  std::string cm_name(CollectionMethodName(config_.GetCollectionMethod()));

  startup_diagnostics.DriverAvailable(cm_name);

  if (system_inspector_.InitKernel(config_)) {
    startup_diagnostics.DriverSuccess(cm_name);
    return true;
  }

  CLOG(ERROR) << "Failed to initialize collector kernel components.";
  // No candidate managed to create a working collector service.
  return false;
}

}  // namespace collector
