#ifndef _COLLECTOR_SERVICE_H_
#define _COLLECTOR_SERVICE_H_

#include <vector>

#include "CollectorConfig.h"
#include "CollectorStats.h"

namespace collector {

class CollectorService {
 public:
  enum ControlValue {
    RUN = 0,           // Keep running
    INTERRUPT_SYSDIG,  // Stop running sysdig, but resume collector operation (e.g., for chisel update)
    STOP_COLLECTOR,    // Stop the collector (e.g., SIGINT or SIGTERM received).
  };

  CollectorService(const CollectorConfig& config, std::atomic<ControlValue>* control, const std::atomic<int>* signum);

  void RunForever();

 private:
  void OnChiselReceived(const std::string& chisel);
  bool WaitForGRPCServer();

  CollectorConfig config_;

  std::string chisel_;
  bool update_chisel_ = false;
  std::mutex chisel_mutex_;

  std::atomic<ControlValue>* control_;
  const std::atomic<int>& signum_;
};

}  // namespace collector

#endif  // _COLLECTOR_SERVICE_H_
