#ifndef _COLLECTOR_SERVICE_H_
#define _COLLECTOR_SERVICE_H_

#include "CollectorConfig.h"
#include "Control.h"
#include "system-inspector/Service.h"

namespace collector {

class CollectorService {
 public:
  CollectorService(const CollectorConfig& config, std::atomic<ControlValue>* control, const std::atomic<int>* signum);

  void RunForever();

  bool InitKernel();

 private:
  bool WaitForGRPCServer();

  CollectorConfig config_;

  std::atomic<ControlValue>* control_;
  const std::atomic<int>& signum_;

  system_inspector::Service system_inspector_;

  void QueryKubelet();
};

bool SetupKernelDriver(CollectorService& collector, const std::string& GRPCServer, const CollectorConfig& config);

}  // namespace collector

#endif  // _COLLECTOR_SERVICE_H_
