#pragma once

#include <atomic>
#include <bitset>
#include <memory>
#include <mutex>

#include <gtest/gtest_prod.h>

#include "ConnTracker.h"
#include "ContainerMetadata.h"
#include "Control.h"
#include "SignalHandler.h"
#include "SignalServiceClient.h"
#include "SystemInspector.h"
#include "events/Dispatcher.h"
#include "events/IEvent.h"

// forward declarations
class sinsp;
class sinsp_evt;
class sinsp_evt_formatter;
class sinsp_threadinfo;

namespace collector::system_inspector {

class Service : public SystemInspector {
 public:
  Service(const Service&) = delete;
  Service(Service&&) = delete;
  Service& operator=(const Service&) = delete;
  Service& operator=(Service&&) = delete;
  ~Service() override;

  Service(const CollectorConfig& config, collector::events::EventDispatcher& dispatcher);
  void Start() override;
  void Run(const std::atomic<ControlValue>& control) override;
  void CleanUp() override;

  bool GetStats(Stats* stats) const override;

  bool InitKernel(const CollectorConfig& config) override;

  using ProcessInfoCallbackRef = std::weak_ptr<std::function<void(std::shared_ptr<sinsp_threadinfo>)>>;

  void GetProcessInformation(uint64_t pid, ProcessInfoCallbackRef callback);

  std::shared_ptr<ContainerMetadata> GetContainerMetadataInspector() { return container_metadata_inspector_; };

  sinsp* GetInspector() { return inspector_.get(); }
  Stats* GetUserspaceStats() { return &userspace_stats_; }

  void AddSignalHandler(std::unique_ptr<SignalHandler> signal_handler);

  events::IEventPtr to_ievt(sinsp_evt* evt);

 private:
  FRIEND_TEST(SystemInspectorServiceTest, FilterEvent);

  struct SignalHandlerEntry {
    std::unique_ptr<SignalHandler> handler;
    std::bitset<PPM_EVENT_MAX> event_filter;

    SignalHandlerEntry(std::unique_ptr<SignalHandler> handler, std::bitset<PPM_EVENT_MAX> event_filter)
        : handler(std::move(handler)), event_filter(event_filter) {}

    bool ShouldHandle(sinsp_evt* evt) const;
  };

  sinsp_evt* GetNext();
  static bool FilterEvent(sinsp_evt* event);
  static bool FilterEvent(const sinsp_threadinfo* tinfo);

  bool SendExistingProcesses(SignalHandler* handler);

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

  collector::events::EventDispatcher& dispatcher_;
};

}  // namespace collector::system_inspector
