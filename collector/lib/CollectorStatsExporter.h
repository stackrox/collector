#ifndef _COLLECTOR_STATS_EXPORTER_H_
#define _COLLECTOR_STATS_EXPORTER_H_

#include <memory>

#include "CollectorConfig.h"
#include "CollectorConnectionStats.h"
#include "CollectorStats.h"
#include "StoppableThread.h"
#include "prometheus/registry.h"
#include "system-inspector/Service.h"

namespace collector {
class CollectorStatsExporter {
 public:
  CollectorStatsExporter(const CollectorConfig* config, system_inspector::Service* si);

  bool start();
  void run();
  void stop();

  std::shared_ptr<prometheus::Registry>& GetRegistry() { return registry_; }

 private:
  std::shared_ptr<prometheus::Registry> registry_;
  const CollectorConfig* config_;
  system_inspector::Service* system_inspector_;
  StoppableThread thread_;
};

}  // namespace collector

#endif  // _COLLECTOR_STATS_EXPORTER_H_
