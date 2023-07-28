#ifndef _COLLECTOR_SERVICE_H_
#define _COLLECTOR_SERVICE_H_

#include <vector>

#include "CollectorConfig.h"
#include "CollectorStats.h"
#include "Control.h"
#include "DriverCandidates.h"
#include "SysdigService.h"

namespace collector {

class SysdigService;

class CollectorService {
 public:
  CollectorService(const CollectorConfig& config, std::atomic<ControlValue>* control, const std::atomic<int>* signum);

  void RunForever();

  bool InitKernel(const DriverCandidate& candidate);

 private:
  void OnChiselReceived(const std::string& chisel);
  bool WaitForGRPCServer();

  CollectorConfig config_;

  std::string chisel_;
  bool update_chisel_ = false;
  std::mutex chisel_mutex_;

  std::atomic<ControlValue>* control_;
  const std::atomic<int>& signum_;

  SysdigService sysdig_;
};

bool SetupKernelDriver(CollectorService& collector, const std::string& GRPCServer, const CollectorConfig& config);

}  // namespace collector

#endif  // _COLLECTOR_SERVICE_H_
