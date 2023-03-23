/** collector

A full notice with attributions is provided along with this source code.

This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License version 2 as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program; if not, write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

* In addition, as a special exception, the copyright holders give
* permission to link the code of portions of this program with the
* OpenSSL library under certain conditions as described in each
* individual source file, and distribute linked combinations
* including the two.
* You must obey the GNU General Public License in all respects
* for all of the code used other than OpenSSL.  If you modify
* file(s) with this exception, you may extend this exception to your
* version of the file(s), but you are not obligated to do so.  If you
* do not wish to do so, delete this exception statement from your
* version.
*/

#include "CollectorService.h"

extern "C" {
#include <signal.h>
}

#include <memory>

#include "CivetServer.h"
#include "CollectorStatsExporter.h"
#include "ConnTracker.h"
#include "Containers.h"
#include "Diagnostics.h"
#include "GRPCUtil.h"
#include "GetKernelObject.h"
#include "GetStatus.h"
#include "LogLevel.h"
#include "NetworkStatusNotifier.h"
#include "ProfilerHandler.h"
#include "SysdigService.h"
#include "Utility.h"
#include "prometheus/exposer.h"

extern unsigned char g_bpf_drop_syscalls[];  // defined in libscap

namespace collector {

CollectorService::CollectorService(const CollectorConfig& config, std::atomic<ControlValue>* control,
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

  GetStatus getStatus(config_.Hostname(), &sysdig_);

  std::shared_ptr<prometheus::Registry> registry = std::make_shared<prometheus::Registry>();

  server.addHandler("/ready", getStatus);
  LogLevelHandler setLogLevel;
  server.addHandler("/loglevel", setLogLevel);

  ProfilerHandler profiler_handler;
  server.addHandler(ProfilerHandler::kBaseRoute, profiler_handler);

  prometheus::Exposer exposer("9090");
  exposer.RegisterCollectable(registry);

  std::unique_ptr<NetworkStatusNotifier> net_status_notifier;

  CLOG(INFO) << "Network scrape interval set to " << config_.ScrapeInterval() << " seconds";

  if (config_.grpc_channel) {
    CLOG(INFO) << "Waiting for Sensor to become ready ...";
    if (!WaitForGRPCServer()) {
      CLOG(INFO) << "Interrupted while waiting for Sensor to become ready ...";
      return;
    }
    CLOG(INFO) << "Sensor connectivity is successful";

    if (!config_.DisableNetworkFlows()) {
      std::shared_ptr<ProcessStore> process_store;
      if (config_.IsProcessesListeningOnPortsEnabled()) {
        process_store = std::make_shared<ProcessStore>(&sysdig_);
      }
      std::shared_ptr<IConnScraper> conn_scraper = std::make_shared<ConnScraper>(config_.HostProc(), process_store);
      conn_tracker = std::make_shared<ConnectionTracker>();
      UnorderedSet<L4ProtoPortPair> ignored_l4proto_port_pairs(config_.IgnoredL4ProtoPortPairs());
      conn_tracker->UpdateIgnoredL4ProtoPortPairs(std::move(ignored_l4proto_port_pairs));

      auto network_connection_info_service_comm = std::make_shared<NetworkConnectionInfoServiceComm>(config_.Hostname(), config_.grpc_channel);

      net_status_notifier = MakeUnique<NetworkStatusNotifier>(conn_scraper, config_.ScrapeInterval(), config_.ScrapeListenEndpoints(), config_.TurnOffScrape(),
                                                              conn_tracker, config_.AfterglowPeriod(), config_.EnableAfterglow(),
                                                              network_connection_info_service_comm);
      net_status_notifier->Start();
    }
  }

  CollectorStatsExporter exporter(registry, &config_, &sysdig_);
  if (!exporter.start()) {
    CLOG(FATAL) << "Unable to start collector stats exporter";
  }

  sysdig_.Init(config_, conn_tracker);
  sysdig_.Start();

  ControlValue cv;
  while ((cv = control_->load(std::memory_order_relaxed)) != STOP_COLLECTOR) {
    sysdig_.Run(*control_);
    CLOG(DEBUG) << "Interrupted collector!";

    std::lock_guard<std::mutex> lock(chisel_mutex_);
    if (update_chisel_) {
      CLOG(DEBUG) << "Updating chisel ...";
      sysdig_.SetChisel(chisel_);
      update_chisel_ = false;
      // Reset the control value to RUN, but abort if it has changed to STOP_COLLECTOR in the meantime.
      cv = control_->exchange(RUN, std::memory_order_relaxed);
      if (cv == STOP_COLLECTOR) {
        break;
      }
    }
  }

  int signal = signum_.load();

  if (signal != 0) {
    CLOG(INFO) << "Caught signal " << signal << " (" << SignalName(signal) << "): " << strsignal(signal);
  }

  CLOG(INFO) << "Shutting down collector.";

  if (net_status_notifier) net_status_notifier->Stop();
  // Shut down these first since they access the sysdig object.
  exporter.stop();
  server.close();

  sysdig_.CleanUp();
}

bool CollectorService::InitKernel(const DriverCandidate& candidate) {
  return sysdig_.InitKernel(config_, candidate);
}

bool CollectorService::WaitForGRPCServer() {
  std::string error_str;
  auto interrupt = [this] { return control_->load(std::memory_order_relaxed) == STOP_COLLECTOR; };
  return WaitForChannelReady(config_.grpc_channel, interrupt);
}

void CollectorService::OnChiselReceived(const std::string& new_chisel) {
  {
    std::lock_guard<std::mutex> lock(chisel_mutex_);
    if (chisel_ == new_chisel) {
      return;
    }

    chisel_ = new_chisel;
    update_chisel_ = true;
  }

  ControlValue cv = RUN;
  control_->compare_exchange_strong(cv, INTERRUPT_SYSDIG, std::memory_order_seq_cst);
}

bool SetupKernelDriver(CollectorService& collector, const std::string& GRPCServer, const CollectorConfig& config) {
  auto& startup_diagnostics = StartupDiagnostics::GetInstance();

  std::vector<DriverCandidate> candidates = GetKernelCandidates(config.GetCollectionMethod());
  if (candidates.empty()) {
    CLOG(ERROR) << "No kernel candidates available";
    return false;
  }

  const char* type = config.UseEbpf() ? "eBPF probe" : "kernel module";
  CLOG(INFO) << "Attempting to find " << type << " - Candidate versions: ";
  for (const auto& candidate : candidates) {
    CLOG(INFO) << candidate.GetName();
  }

  for (const auto& candidate : candidates) {
    if (!GetKernelObject(GRPCServer, config.TLSConfiguration(), candidate, config.CurlVerbose())) {
      CLOG(WARNING) << "No suitable kernel object downloaded for " << candidate.GetName();
      startup_diagnostics.DriverUnavailable(candidate.GetName());
      continue;
    }

    startup_diagnostics.DriverAvailable(candidate.GetName());

    if (collector.InitKernel(candidate)) {
      startup_diagnostics.DriverSuccess(candidate.GetName());
      return true;
    } else if (candidate.GetCollectionMethod() == KERNEL_MODULE) {
      // Kernel module drops capabilities, so subsequent attempts could fail
      // instead we use the legacy behaviour of failing.
      break;
    }
  }

  CLOG(ERROR) << "Failed to initialize collector kernel components.";
  // No candidate managed to create a working collector service.
  return false;
}

}  // namespace collector
