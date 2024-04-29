#ifndef _SYSTEM_INSPECTOR_SERVICE_H_
#define _SYSTEM_INSPECTOR_SERVICE_H_

#include <atomic>
#include <bitset>
#include <memory>
#include <mutex>
#include <string>

#include <gtest/gtest_prod.h>

#include "libsinsp/sinsp.h"

#include "ContainerMetadata.h"
#include "Control.h"
#include "DriverCandidates.h"
#include "SignalHandler.h"
#include "SignalServiceClient.h"
#include "SystemInspector.h"
#include "threadinfo.h"

namespace collector::system_inspector {

class Service : public SystemInspector {
 public:
  static constexpr char kModulePath[] = "/module/collector.ko";
  static constexpr char kModuleName[] = "collector";
  static constexpr char kProbePath[] = "/module/collector-ebpf.o";
  static constexpr char kProbeName[] = "collector-ebpf";
  static constexpr int kMessageBufferSize = 8192;
  static constexpr int kKeyBufferSize = 48;

  Service() = default;

  void Init(const CollectorConfig& config, std::shared_ptr<ConnectionTracker> conn_tracker) override;
  void Start() override;
  void Run(const std::atomic<ControlValue>& control) override;
  void CleanUp() override;

  bool GetStats(Stats* stats) const override;

  bool InitKernel(const CollectorConfig& config, const DriverCandidate& candidate) override;

  typedef std::weak_ptr<std::function<void(threadinfo_map_t::ptr_t)>> ProcessInfoCallbackRef;

  void GetProcessInformation(uint64_t pid, ProcessInfoCallbackRef callback);

  std::shared_ptr<ContainerMetadata> GetContainerMetadataInspector() { return container_metadata_inspector_; };

 private:
  FRIEND_TEST(SystemInspectorServiceTest, FilterEvent);

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
  static bool FilterEvent(sinsp_evt* event);
  static bool FilterEvent(const sinsp_threadinfo* tinfo);

  bool SendExistingProcesses(SignalHandler* handler);

  void AddSignalHandler(std::unique_ptr<SignalHandler> signal_handler);

  mutable std::mutex libsinsp_mutex_;
  std::unique_ptr<sinsp> inspector_;
  std::shared_ptr<ContainerMetadata> container_metadata_inspector_;
  std::unique_ptr<sinsp_evt_formatter> default_formatter_;
  std::unique_ptr<ISignalServiceClient> signal_client_;
  std::vector<SignalHandlerEntry> signal_handlers_;
  Stats userspace_stats_;
  std::bitset<PPM_EVENT_MAX> global_event_filter_;

  mutable std::mutex running_mutex_;
  bool running_ = false;

  void ServePendingProcessRequests();
  mutable std::mutex process_requests_mutex_;
  // [ ( pid, callback ), ( pid, callback ), ... ]
  std::list<std::pair<uint64_t, ProcessInfoCallbackRef>> pending_process_requests_;
};

}  // namespace collector::system_inspector

#endif  // _SYSTEM_INSPECTOR_SERVICE_H_
