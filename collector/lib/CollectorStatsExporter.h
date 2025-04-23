#pragma once

#include <memory>

#include "CollectorConfig.h"
#include "CollectorStats.h"
#include "StoppableThread.h"
#include "prometheus/registry.h"
#include "system-inspector/Service.h"

namespace collector {

class CollectorStatsExporter {
 public:
  CollectorStatsExporter(std::shared_ptr<prometheus::Registry> registry, const CollectorConfig* config, system_inspector::Service* si);

  bool start();
  void run();
  void stop();

  std::shared_ptr<CollectorConnectionStats<unsigned int>> GetConnectionsTotalReporter() { return connections_total_reporter_; }
  std::shared_ptr<CollectorConnectionStats<float>> GetConnectionsRateReporter() { return connections_rate_reporter_; }

 private:
  std::shared_ptr<prometheus::Registry> registry_;
  const CollectorConfig* config_;
  system_inspector::Service* system_inspector_;
  std::shared_ptr<CollectorConnectionStats<unsigned int>> connections_total_reporter_;
  std::shared_ptr<CollectorConnectionStats<float>> connections_rate_reporter_;
  StoppableThread thread_;
};

}  // namespace collector
