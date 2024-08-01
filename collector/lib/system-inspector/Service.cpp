#include "Service.h"

#include <cap-ng.h>
#include <thread>

#include <linux/ioctl.h>

#include "libsinsp/container_engine/sinsp_container_type.h"
#include "libsinsp/parsers.h"
#include "libsinsp/sinsp.h"

#include <google/protobuf/util/time_util.h>

#include "CollectionMethod.h"
#include "CollectorException.h"
#include "CollectorStats.h"
#include "ContainerEngine.h"
#include "ContainerMetadata.h"
#include "EventExtractor.h"
#include "EventNames.h"
#include "HostInfo.h"
#include "KernelDriver.h"
#include "Logging.h"
#include "NetworkSignalHandler.h"
#include "ProcessSignalHandler.h"
#include "SelfCheckHandler.h"
#include "SelfChecks.h"
#include "TimeUtil.h"
#include "Utility.h"
#include "logger.h"

namespace collector::system_inspector {

constexpr char Service::kModulePath[];
constexpr char Service::kModuleName[];
constexpr char Service::kProbePath[];
constexpr char Service::kProbeName[];

Service::Service() {}
Service::~Service() {}

void Service::Init(const CollectorConfig& config, std::shared_ptr<ConnectionTracker> conn_tracker) {
  // The self-check handlers should only operate during start up,
  // so they are added to the handler list first, so they have access
  // to self-check events before the network and process handlers have
  // a chance to process them and send them to Sensor.
  AddSignalHandler(MakeUnique<SelfCheckProcessHandler>(inspector_.get()));
  AddSignalHandler(MakeUnique<SelfCheckNetworkHandler>(inspector_.get()));

  if (conn_tracker) {
    auto network_signal_handler_ = MakeUnique<NetworkSignalHandler>(inspector_.get(), conn_tracker, &userspace_stats_);

    network_signal_handler_->SetCollectConnectionStatus(config.CollectConnectionStatus());

    AddSignalHandler(std::move(network_signal_handler_));
  }

  if (config.grpc_channel) {
    signal_client_.reset(new SignalServiceClient(std::move(config.grpc_channel)));
  } else {
    signal_client_.reset(new StdoutSignalServiceClient());
  }
  AddSignalHandler(MakeUnique<ProcessSignalHandler>(inspector_.get(),
                                                    signal_client_.get(),
                                                    &userspace_stats_));

  if (signal_handlers_.size() == 2) {
    // self-check handlers do not count towards this check, because they
    // do not send signals to Sensor.
    CLOG(FATAL) << "Internal error: There are no signal handlers.";
  }
}

bool Service::InitKernel(const CollectorConfig& config, const DriverCandidate& candidate) {
  if (!inspector_) {
    inspector_.reset(new sinsp());

    // peeking into arguments has a big overhead, so we prevent it from happening
    inspector_->set_snaplen(0);

    auto log_level = (sinsp_logger::severity)logging::GetLogLevel();
    inspector_->set_min_log_severity(log_level);
    inspector_->disable_log_timestamps();
    inspector_->set_log_callback(logging::InspectorLogCallback);

    inspector_->set_import_users(config.ImportUsers(), false);
    inspector_->set_thread_timeout_s(30);
    inspector_->set_auto_threads_purging_interval_s(60);
    inspector_->m_thread_manager->set_max_thread_table_size(config.GetSinspThreadCacheSize());

    // enable stats v2
    inspector_->set_sinsp_stats_v2_enabled();

    // Connection status tracking is used in NetworkSignalHandler,
    // but only when trying to handle asynchronous connections
    // as a special case.
    if (config.CollectConnectionStatus()) {
      inspector_->get_parser()->set_track_connection_status(true);
    }

    container_metadata_inspector_.reset(new ContainerMetadata(inspector_.get()));

    if (config.EnableRuntimeConfig()) {
      uint64_t mask = 1 << CT_CRI |
                      1 << CT_CRIO |
                      1 << CT_CONTAINERD;

      if (config.UseDockerCe()) {
        mask |= 1 << CT_DOCKER;
      }

      if (config.UsePodmanCe()) {
        mask |= 1 << CT_PODMAN;
      }

      inspector_->set_container_engine_mask(mask);

      // k8s naming conventions specify that max length be 253 characters
      // (the extra 2 are just for a nice 0xFF).
      inspector_->set_container_labels_max_len(255);
    } else {
      auto engine = std::make_shared<ContainerEngine>(inspector_->m_container_manager);
      auto* container_engines = inspector_->m_container_manager.get_container_engines();
      container_engines->push_back(engine);
    }

    inspector_->set_filter("container.id != 'host'");

    default_formatter_.reset(new sinsp_evt_formatter(inspector_.get(),
                                                     DEFAULT_OUTPUT_STR,
                                                     EventExtractor::FilterList()));
  }

  KernelDriverCOREEBPF driver;
  if (!driver.Setup(config, *inspector_)) {
    CLOG(ERROR) << "Failed to setup " << candidate.GetName();
    return false;
  }

  return true;
}

sinsp_evt* Service::GetNext() {
  std::lock_guard<std::mutex> lock(libsinsp_mutex_);
  sinsp_evt* event = nullptr;

  auto parse_start = NowMicros();
  auto res = inspector_->next(&event);
  if (res != SCAP_SUCCESS || event == nullptr) return nullptr;

#ifdef TRACE_SINSP_EVENTS
  // Do not allow to change sinsp events tracing at runtime, as the output
  // could contain some sensitive information and it's not worth risking
  // misconfiguration.
  //
  // Wrap the whole thing into the log level condition to avoid unnecessary
  // overhead, as there will be tons of events.
  if (logging::GetLogLevel() == logging::LogLevel::TRACE) {
    std::string output;
    default_formatter_->tostring(event, output);
    CLOG(TRACE) << output;
  }
#endif

  if (event->get_category() & EC_INTERNAL) return nullptr;

  HostInfo& host_info = HostInfo::Instance();

  // This additional userspace filter is a guard against additional events
  // from the eBPF probe. This can occur when using sys_enter and sys_exit
  // tracepoints rather than a targeted approach, which we currently only do
  // on RHEL7 with backported eBPF
  if (host_info.IsRHEL76() && !global_event_filter_[event->get_type()]) {
    return nullptr;
  }

  userspace_stats_.event_parse_micros[event->get_type()] += (NowMicros() - parse_start);
  ++userspace_stats_.nUserspaceEvents[event->get_type()];

  if (!FilterEvent(event)) {
    return nullptr;
  }
  ++userspace_stats_.nFilteredEvents[event->get_type()];

  return event;
}

bool Service::FilterEvent(sinsp_evt* event) {
  const auto* tinfo = event->get_thread_info();

  return FilterEvent(tinfo);
}

bool Service::FilterEvent(const sinsp_threadinfo* tinfo) {
  if (tinfo == nullptr) {
    return false;
  }

  // exclude runc events
  if ((tinfo->m_exepath == "runc" ||
       tinfo->m_exepath == "/usr/bin/runc") &&
      tinfo->m_comm == "6") {
    return false;
  }

  std::string_view exepath_sv{tinfo->m_exepath};
  auto marker = exepath_sv.rfind(':');
  if (marker != std::string_view::npos) {
    exepath_sv.remove_prefix(marker + 1);
  }

  return exepath_sv.rfind("/proc/self", 0) != 0;
}

void Service::Start() {
  std::lock_guard<std::mutex> libsinsp_lock(libsinsp_mutex_);

  if (!inspector_) {
    throw CollectorException("Invalid state: system inspector was not initialized");
  }

  for (auto& signal_handler : signal_handlers_) {
    if (!signal_handler.handler->Start()) {
      CLOG(FATAL) << "Error starting signal handler " << signal_handler.handler->GetName();
    }
  }

  inspector_->start_capture();

  // trigger the self check process only once capture has started,
  // to verify the driver is working correctly. SelfCheckHandlers will
  // verify the live events.
  std::thread self_checks_thread(self_checks::start_self_check_process);
  self_checks_thread.detach();

  std::lock_guard<std::mutex> running_lock(running_mutex_);
  running_ = true;
}

void LogUnreasonableEventTime(int64_t time_micros, sinsp_evt* evt) {
  int64_t time_diff;
  int64_t evt_ts = evt->get_ts() / 1000UL;
  int64_t max_past_time = 3600000000;  // One hour in microseconds. This is generous.
  int64_t max_future_time = 5000000;   // 5 seconds in microseconds. A time in a little in the future is suspicious.

  time_diff = time_micros - evt_ts;
  if (time_diff > max_past_time) {
    CLOG_THROTTLED(WARNING, std::chrono::seconds(1800)) << "Event of type " << evt->get_type() << " is unreasonably old. It's timestamp is " << google::protobuf::util::TimeUtil::MicrosecondsToTimestamp(evt_ts);
    COUNTER_INC(CollectorStats::event_timestamp_distant_past);
  }

  if (time_diff < -max_future_time) {
    CLOG_THROTTLED(WARNING, std::chrono::seconds(1800)) << "Event of type " << evt->get_type() << " is in the future. It's timestamp is " << google::protobuf::util::TimeUtil::MicrosecondsToTimestamp(evt_ts);
    COUNTER_INC(CollectorStats::event_timestamp_future);
  }
}

void Service::Run(const std::atomic<ControlValue>& control) {
  if (!inspector_) {
    throw CollectorException("Invalid state: system inspector was not initialized");
  }

  while (control.load(std::memory_order_relaxed) == ControlValue::RUN) {
    ServePendingProcessRequests();

    sinsp_evt* evt = GetNext();
    if (!evt) continue;

    auto process_start = NowMicros();
    for (auto it = signal_handlers_.begin(); it != signal_handlers_.end(); it++) {
      auto& signal_handler = *it;
      if (!signal_handler.ShouldHandle(evt)) continue;
      LogUnreasonableEventTime(process_start, evt);
      auto result = signal_handler.handler->HandleSignal(evt);
      if (result == SignalHandler::NEEDS_REFRESH) {
        if (!SendExistingProcesses(signal_handler.handler.get())) {
          continue;
        }
        result = signal_handler.handler->HandleSignal(evt);
      } else if (result == SignalHandler::FINISHED) {
        // This signal handler has finished processing events,
        // so remove it from the signal handler list.
        //
        // We don't need to update the iterator post-deletion
        // because we also stop iteration at this point.
        signal_handlers_.erase(it);
        break;
      }
    }

    userspace_stats_.event_process_micros[evt->get_type()] += (NowMicros() - process_start);
  }
}

bool Service::SendExistingProcesses(SignalHandler* handler) {
  std::lock_guard<std::mutex> lock(libsinsp_mutex_);

  if (!inspector_) {
    throw CollectorException("Invalid state: system inspector was not initialized");
  }

  auto threads = inspector_->m_thread_manager->get_threads();
  if (!threads) {
    CLOG(WARNING) << "Null thread manager";
    return false;
  }

  return threads->loop([&](sinsp_threadinfo& tinfo) {
    if (!tinfo.m_container_id.empty() && tinfo.is_main_thread()) {
      auto result = handler->HandleExistingProcess(&tinfo);
      if (result == SignalHandler::ERROR || result == SignalHandler::NEEDS_REFRESH) {
        CLOG(WARNING) << "Failed to write existing process signal: " << &tinfo;
        return false;
      }
      CLOG(DEBUG) << "Found existing process: " << &tinfo;
    }
    return true;
  });
}

void Service::CleanUp() {
  std::lock_guard<std::mutex> libsinsp_lock(libsinsp_mutex_);
  std::lock_guard<std::mutex> running_lock(running_mutex_);
  running_ = false;
  inspector_->close();
  inspector_.reset();

  for (auto& signal_handler : signal_handlers_) {
    if (!signal_handler.handler->Stop()) {
      CLOG(ERROR) << "Error stopping signal handler " << signal_handler.handler->GetName();
    }
  }

  signal_handlers_.clear();

  // Cancel all pending process requests
  std::lock_guard<std::mutex> lock(process_requests_mutex_);

  while (!pending_process_requests_.empty()) {
    auto& request = pending_process_requests_.front();
    auto callback = request.second.lock();

    if (callback) (*callback)(0);

    pending_process_requests_.pop_front();
  }
}

bool Service::GetStats(system_inspector::Stats* stats) const {
  std::lock_guard<std::mutex> libsinsp_lock(libsinsp_mutex_);
  std::lock_guard<std::mutex> running_lock(running_mutex_);
  if (!running_ || !inspector_) return false;

  scap_stats kernel_stats;
  std::shared_ptr<const sinsp_stats_v2> userspace_stats;

  inspector_->get_capture_stats(&kernel_stats);
  userspace_stats = inspector_->get_sinsp_stats_v2();

  *stats = userspace_stats_;
  stats->nEvents = kernel_stats.n_evts;
  stats->nDrops = kernel_stats.n_drops;
  stats->nDropsBuffer = kernel_stats.n_drops_buffer;
  stats->nPreemptions = kernel_stats.n_preemptions;
  stats->nThreadCacheSize = inspector_->m_thread_manager->get_thread_count();

  if (userspace_stats != nullptr)
    stats->nDropsThreadCache = userspace_stats->m_n_drops_full_threadtable;

  return true;
}

void Service::AddSignalHandler(std::unique_ptr<SignalHandler> signal_handler) {
  std::bitset<PPM_EVENT_MAX> event_filter;
  const auto& relevant_events = signal_handler->GetRelevantEvents();
  if (relevant_events.empty()) {
    event_filter.set();
  } else {
    const EventNames& event_names = EventNames::GetInstance();
    for (const auto& event_name : relevant_events) {
      for (ppm_event_code event_id : event_names.GetEventIDs(event_name)) {
        event_filter.set(event_id);
        global_event_filter_.set(event_id);
      }
    }
  }

  signal_handlers_.emplace_back(std::move(signal_handler), event_filter);
}

void Service::GetProcessInformation(uint64_t pid, ProcessInfoCallbackRef callback) {
  std::lock_guard<std::mutex> lock(process_requests_mutex_);

  pending_process_requests_.emplace_back(pid, callback);
}

void Service::ServePendingProcessRequests() {
  std::lock_guard<std::mutex> lock(process_requests_mutex_);

  while (!pending_process_requests_.empty()) {
    auto& request = pending_process_requests_.front();
    uint64_t pid = request.first;
    auto callback = request.second.lock();

    if (callback) {
      (*callback)(inspector_->get_thread_ref(pid, true));
    }

    pending_process_requests_.pop_front();
  }
}

bool Service::SignalHandlerEntry::ShouldHandle(sinsp_evt* evt) const {
  return event_filter[evt->get_type()];
}

}  // namespace collector::system_inspector
