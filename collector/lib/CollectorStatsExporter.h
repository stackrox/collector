#pragma once

#include <memory>

#include "CollectorConfig.h"
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
