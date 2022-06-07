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

#include "CollectorStatsExporter.h"
#include "ConnTracker.h"
#include "Containers.h"
#include "GRPCUtil.h"
#include "GetStatus.h"
#include "LogLevel.h"
#include "NetworkStatusNotifier.h"
#include "ProfilerHandler.h"
#include "SysdigService.h"
#include "Utility.h"
#include "civetweb/CivetServer.h"
#include "prometheus/exposer.h"

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

  SysdigService sysdig;
  GetStatus getStatus(config_.Hostname(), &sysdig);

  std::shared_ptr<prometheus::Registry> registry = std::make_shared<prometheus::Registry>();

  server.addHandler("/ready", getStatus);
  LogLevel setLogLevel;
  server.addHandler("/loglevel", setLogLevel);

  ProfilerHandler profiler_handler;
  server.addHandler(ProfilerHandler::kBaseRoute, profiler_handler);

  prometheus::Exposer exposer("9090");
  exposer.RegisterCollectable(registry);

  std::unique_ptr<NetworkStatusNotifier> net_status_notifier;

  CLOG(INFO) << "Network scrape interval set to " << config_.ScrapeInterval() << " seconds";

  if (config_.grpc_channel) {
    CLOG(INFO) << "Waiting for GRPC server to become ready ...";
    if (!WaitForGRPCServer()) {
      CLOG(INFO) << "Interrupted while waiting for GRPC server to become ready ...";
      return;
    }
    CLOG(INFO) << "GRPC server connectivity is successful";

    if (!config_.DisableNetworkFlows()) {
      std::shared_ptr<IConnScraper> conn_scraper = std::make_shared<ConnScraper>(config_.HostProc());
      conn_tracker = std::make_shared<ConnectionTracker>();
      UnorderedSet<L4ProtoPortPair> ignored_l4proto_port_pairs(config_.IgnoredL4ProtoPortPairs());
      conn_tracker->UpdateIgnoredL4ProtoPortPairs(std::move(ignored_l4proto_port_pairs));
      net_status_notifier = MakeUnique<NetworkStatusNotifier>(config_.Hostname(), conn_scraper,
                                                              config_.ScrapeInterval(), config_.ScrapeListenEndpoints(),
                                                              config_.TurnOffScrape(),
                                                              conn_tracker, config_.grpc_channel,
                                                              config_.AfterglowPeriod(), config_.EnableAfterglow());
      net_status_notifier->Start();
    }
  }

  sysdig.Init(config_, conn_tracker);

  CollectorStatsExporter exporter(registry, &config_, &sysdig);
  if (!exporter.start()) {
    CLOG(FATAL) << "Unable to start collector stats exporter";
  }

  sysdig.Start();

  ControlValue cv;
  while ((cv = control_->load(std::memory_order_relaxed)) != STOP_COLLECTOR) {
    sysdig.Run(*control_);
    CLOG(INFO) << "Interrupted collector!";

    std::lock_guard<std::mutex> lock(chisel_mutex_);
    if (update_chisel_) {
      CLOG(DEBUG) << "Updating chisel ...";
      sysdig.SetChisel(chisel_);
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

  sysdig.CleanUp();
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

}  // namespace collector
