#ifndef _COLLECTOR_STATS_EXPORTER_H_
#define _COLLECTOR_STATS_EXPORTER_H_

#include <memory>
#include <utility>

#include "CollectorConfig.h"
#include "CollectorStats.h"
#include "StoppableThread.h"
#include "prometheus/registry.h"
#include "system-inspector/Service.h"

namespace collector {

class CollectorStatsExporter {
 public:
  CollectorStatsExporter() = default;
  CollectorStatsExporter(const CollectorStatsExporter&) = delete;
  CollectorStatsExporter(CollectorStatsExporter&&) = delete;
  CollectorStatsExporter& operator=(const CollectorStatsExporter&) = delete;
  ~CollectorStatsExporter() = default;

  CollectorStatsExporter& operator=(CollectorStatsExporter&& other) noexcept {
    auto swap_running = other.thread_.running();

    if (swap_running) {
      other.stop();
    }

    registry_.swap(other.registry_);
    std::swap(config_, other.config_);
    std::swap(system_inspector_, other.system_inspector_);
    connections_total_reporter_.swap(other.connections_total_reporter_);
    connections_rate_reporter_.swap(other.connections_rate_reporter_);

    if (swap_running) {
      start();
    }

    return *this;
  }

  CollectorStatsExporter(std::shared_ptr<prometheus::Registry> registry, const CollectorConfig* config, system_inspector::Service* si);

  bool start();
  void run();
  void stop();

  std::shared_ptr<CollectorConnectionStats<unsigned int>> GetConnectionsTotalReporter() { return connections_total_reporter_; }
  std::shared_ptr<CollectorConnectionStats<float>> GetConnectionsRateReporter() { return connections_rate_reporter_; }

 private:
  std::shared_ptr<prometheus::Registry> registry_;
  const CollectorConfig* config_{};
  system_inspector::Service* system_inspector_{};
  std::shared_ptr<CollectorConnectionStats<unsigned int>> connections_total_reporter_;
  std::shared_ptr<CollectorConnectionStats<float>> connections_rate_reporter_;
  StoppableThread thread_;
};

}  // namespace collector

#endif  // _COLLECTOR_STATS_EXPORTER_H_
