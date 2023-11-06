#ifndef _COLLECTOR_STATS_EXPORTER_H_
#define _COLLECTOR_STATS_EXPORTER_H_

#include <memory>

#include "CollectorConfig.h"
#include "CollectorStats.h"
#include "StoppableThread.h"
#include "SysdigService.h"
#include "prometheus/registry.h"

namespace collector {

class CollectorStatsExporter {
 public:
  CollectorStatsExporter(std::shared_ptr<prometheus::Registry> registry, const CollectorConfig* config, SysdigService* sysdig);

  bool start();
  void run();
  void stop();

  std::shared_ptr<CollectorConnectionStats<unsigned int>> GetConnectionsTotalReporter() { return connections_total_reporter_; }
  std::shared_ptr<CollectorConnectionStats<float>> GetConnectionsRateReporter() { return connections_rate_reporter_; }

 private:
  std::shared_ptr<prometheus::Registry> registry_;
  const CollectorConfig* config_;
  SysdigService* sysdig_;
  std::shared_ptr<CollectorConnectionStats<unsigned int>> connections_total_reporter_;
  std::shared_ptr<CollectorConnectionStats<float>> connections_rate_reporter_;
  StoppableThread thread_;
};

}  // namespace collector

#endif  // _COLLECTOR_STATS_EXPORTER_H_
