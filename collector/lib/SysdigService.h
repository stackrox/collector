#ifndef _SYSDIG_SERVICE_H_
#define _SYSDIG_SERVICE_H_

#include <atomic>
#include <bitset>
#include <memory>
#include <mutex>
#include <string>

// clang-format off
// sinsp.h needs to be included before chisel.h
#include "libsinsp/sinsp.h"
#include "chisel.h"
// clang-format on

#include "CollectorService.h"
#include "SignalHandler.h"
#include "Sysdig.h"

namespace collector {

class SysdigService : public Sysdig {
 public:
  static constexpr char kModulePath[] = "/module/collector.ko";
  static constexpr char kModuleName[] = "collector";
  static constexpr char kProbePath[] = "/module/collector-ebpf.o";
  static constexpr char kProbeName[] = "collector-ebpf";
  static constexpr int kMessageBufferSize = 8192;
  static constexpr int kKeyBufferSize = 48;

  SysdigService() = default;

  void Init(const CollectorConfig& config, std::shared_ptr<ConnectionTracker> conn_tracker) override;
  void Start() override;
  void Run(const std::atomic<CollectorService::ControlValue>& control) override;
  void SetChisel(const std::string& new_chisel);
  void CleanUp() override;

  bool GetStats(SysdigStats* stats) const override;

 private:
  enum ChiselCacheStatus : int {
    BLOCKED_USERSPACE,
    BLOCKED_KERNEL,
    ACCEPTED,
  };

  struct SignalHandlerEntry {
    std::unique_ptr<SignalHandler> handler;
    std::bitset<PPM_EVENT_MAX> event_filter;

    SignalHandlerEntry(std::unique_ptr<SignalHandler> handler, std::bitset<PPM_EVENT_MAX> event_filter)
        : handler(std::move(handler)), event_filter(event_filter) {}

    bool ShouldHandle(sinsp_evt* evt) const {
      return event_filter[evt->get_type()];
    }
  };

  sinsp_evt* GetNext();

  bool FilterEvent(sinsp_evt* event);
  bool SendExistingProcesses(SignalHandler* handler);

  void AddSignalHandler(std::unique_ptr<SignalHandler> signal_handler);

  std::unique_ptr<sinsp> inspector_;
  std::unique_ptr<sinsp_chisel> chisel_;
  std::vector<SignalHandlerEntry> signal_handlers_;
  SysdigStats userspace_stats_;
  std::bitset<PPM_EVENT_MAX> global_event_filter_;

  std::unordered_map<string, ChiselCacheStatus> chisel_cache_;
  bool use_chisel_cache_;

  mutable std::mutex running_mutex_;
  bool running_ = false;
  bool useEbpf = false;
};

}  // namespace collector

#endif  // _SYSDIG_SERVICE_H_
