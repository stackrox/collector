/** collector

A full notice with attributions is provided along with this source code.

This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License version 2 as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program; if not, write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

* In addition, as a special exception, the copyright holders give
* permission to link the code of portions of this program with the
* OpenSSL library under certain conditions as described in each
* individual source file, and distribute linked combinations
* including the two.
* You must obey the GNU General Public License in all respects
* for all of the code used other than OpenSSL.  If you modify
* file(s) with this exception, you may extend this exception to your
* version of the file(s), but you are not obligated to do so.  If you
* do not wish to do so, delete this exception statement from your
* version.
*/

#include "SysdigService.h"

#include <cap-ng.h>

#include <linux/ioctl.h>

#include "libsinsp/wrapper.h"

#include "CollectorException.h"
#include "EventNames.h"
#include "HostInfo.h"
#include "KernelDriver.h"
#include "Logging.h"
#include "NetworkSignalHandler.h"
#include "ProcessSignalHandler.h"
#include "TimeUtil.h"
#include "Utility.h"

namespace collector {

constexpr char SysdigService::kModulePath[];
constexpr char SysdigService::kModuleName[];
constexpr char SysdigService::kProbePath[];
constexpr char SysdigService::kProbeName[];

void SysdigService::Init(const CollectorConfig& config, std::shared_ptr<ConnectionTracker> conn_tracker) {
  if (chisel_) {
    throw CollectorException("Invalid state: SysdigService was already initialized");
  }

  if (conn_tracker) {
    AddSignalHandler(MakeUnique<NetworkSignalHandler>(inspector_.get(), conn_tracker, &userspace_stats_));
  }

  if (config.grpc_channel) {
    AddSignalHandler(MakeUnique<ProcessSignalHandler>(inspector_.get(), config.grpc_channel, &userspace_stats_));
  }

  if (signal_handlers_.empty()) {
    CLOG(FATAL) << "Internal error: There are no signal handlers.";
  }

  SetChisel(config.Chisel());

  use_chisel_cache_ = config.UseChiselCache();
}

bool SysdigService::InitKernel(const CollectorConfig& config) {
  if (inspector_) {
    throw CollectorException("Invalid state: SysdigService kernel components are already initialized");
  }

  inspector_.reset(new_inspector());
  inspector_->set_snaplen(config.SnapLen());

  if (config.EnableSysdigLog()) {
    inspector_->set_log_stderr();
  }

  if (config.UseEbpf()) {
    useEbpf_ = true;
    KernelDriverEBPF driver;
    if (!driver.Setup(config, SysdigService::kProbePath)) {
      CLOG(ERROR) << "Failed to setup eBPF probe";
      return false;
    }
  } else {
    KernelDriverModule driver;
    if (!driver.Setup(config, SysdigService::kModulePath)) {
      CLOG(ERROR) << "Failed to setup Kernel module";
      return false;
    }
  }

  return true;
}

bool SysdigService::FilterEvent(sinsp_evt* event) {
  if (!use_chisel_cache_) {
    return chisel_->process(event);
  }

  sinsp_threadinfo* tinfo = event->get_thread_info();
  if (!tinfo || tinfo->m_container_id.empty()) {
    return false;
  }

  auto pair = chisel_cache_.emplace(tinfo->m_container_id, ACCEPTED);
  ChiselCacheStatus& cache_status = pair.first->second;
  bool res;

  if (pair.second) {  // was newly inserted
    res = chisel_->process(event);
    if (chisel_cache_.size() > 1024) {
      CLOG(INFO) << "Flushing chisel cache";
      chisel_cache_.clear();
      return res;
    }
    cache_status = res ? ACCEPTED : BLOCKED_USERSPACE;
  } else {
    res = (cache_status == ACCEPTED);

    if (res) {
      ++userspace_stats_.nChiselCacheHitsAccept[event->get_type()];
    } else {
      ++userspace_stats_.nChiselCacheHitsReject[event->get_type()];
    }
  }

  if (!useEbpf_) {
    if (cache_status == BLOCKED_USERSPACE && event->get_type() != PPME_PROCEXIT_1_E) {
      if (!inspector_->ioctl(0, PPM_IOCTL_EXCLUDE_NS_OF_PID, reinterpret_cast<void*>(tinfo->m_pid))) {
        CLOG(WARNING) << "Failed to exclude namespace for pid " << tinfo->m_pid << ": " << inspector_->getlasterr();
      } else {
        cache_status = BLOCKED_KERNEL;
      }
    }
  }

  return res;
}

sinsp_evt* SysdigService::GetNext() {
  sinsp_evt* event;

  auto parse_start = NowMicros();
  auto res = inspector_->next(&event);
  if (res != SCAP_SUCCESS) return nullptr;

  if (event->get_category() & EC_INTERNAL) return nullptr;

  HostInfo& host_info = HostInfo::Instance();

  // This additional userspace filter is a guard against additional events
  // from the eBPF probe. This can occur when using sys_enter and sys_exit
  // tracepoints rather than a targeted approach, which we currently only do
  // on RHEL7 with backported eBPF
  if (useEbpf_ && host_info.IsRHEL76() && !global_event_filter_[event->get_type()]) {
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

void SysdigService::Start() {
  if (!inspector_ || !chisel_) {
    throw CollectorException("Invalid state: SysdigService was not initialized");
  }

  for (auto& signal_handler : signal_handlers_) {
    if (!signal_handler.handler->Start()) {
      CLOG(FATAL) << "Error starting signal handler " << signal_handler.handler->GetName();
    }
  }

  /* Get only necessary tracepoints. */
  std::unordered_set<uint32_t> tp_set = inspector_->enforce_sinsp_state_tp();
  std::unordered_set<uint32_t> ppm_sc;

  if (!useEbpf_) {
    inspector_->open_kmod(DEFAULT_DRIVER_BUFFER_BYTES_DIM, ppm_sc, tp_set);

    // Drop DAC_OVERRIDE capability after opening the device files.
    capng_updatev(CAPNG_DROP, static_cast<capng_type_t>(CAPNG_EFFECTIVE | CAPNG_PERMITTED), CAP_DAC_OVERRIDE, -1);
    if (capng_apply(CAPNG_SELECT_BOTH) != 0) {
      CLOG(WARNING) << "Failed to drop DAC_OVERRIDE capability: " << StrError();
    }
  } else {
    inspector_->open_bpf(kProbePath, DEFAULT_DRIVER_BUFFER_BYTES_DIM, ppm_sc, tp_set);
  }

  std::lock_guard<std::mutex> lock(running_mutex_);
  running_ = true;
}

void SysdigService::Run(const std::atomic<ControlValue>& control) {
  if (!inspector_ || !chisel_) {
    throw CollectorException("Invalid state: SysdigService was not initialized");
  }

  while (control.load(std::memory_order_relaxed) == ControlValue::RUN) {
    sinsp_evt* evt = GetNext();
    if (!evt) continue;

    auto process_start = NowMicros();
    for (auto& signal_handler : signal_handlers_) {
      if (!signal_handler.ShouldHandle(evt)) continue;
      auto result = signal_handler.handler->HandleSignal(evt);
      if (result == SignalHandler::NEEDS_REFRESH) {
        if (!SendExistingProcesses(signal_handler.handler.get())) {
          continue;
        }
        result = signal_handler.handler->HandleSignal(evt);
      }
    }

    userspace_stats_.event_process_micros[evt->get_type()] += (NowMicros() - process_start);
  }
}

bool SysdigService::SendExistingProcesses(SignalHandler* handler) {
  if (!inspector_ || !chisel_) {
    throw CollectorException("Invalid state: SysdigService was not initialized");
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

void SysdigService::CleanUp() {
  std::lock_guard<std::mutex> lock(running_mutex_);
  running_ = false;
  inspector_->close();
  chisel_.reset();
  inspector_.reset();

  for (auto& signal_handler : signal_handlers_) {
    if (!signal_handler.handler->Stop()) {
      CLOG(ERROR) << "Error stopping signal handler " << signal_handler.handler->GetName();
    }
  }

  signal_handlers_.clear();
}

bool SysdigService::GetStats(SysdigStats* stats) const {
  std::lock_guard<std::mutex> lock(running_mutex_);
  if (!running_ || !inspector_) return false;

  scap_stats kernel_stats;
  inspector_->get_capture_stats(&kernel_stats);
  *stats = userspace_stats_;
  stats->nEvents = kernel_stats.n_evts;
  stats->nDrops = kernel_stats.n_drops;
  stats->nPreemptions = kernel_stats.n_preemptions;

  return true;
}

void SysdigService::SetChisel(const std::string& chisel) {
  CLOG(DEBUG) << "Updating chisel and flushing chisel cache";
  CLOG(DEBUG) << "New chisel: " << chisel;
  chisel_.reset(new_chisel(inspector_.get(), chisel, false));
  chisel_->on_init();
  chisel_cache_.clear();

  if (!useEbpf_) {
    std::lock_guard<std::mutex> lock(running_mutex_);
    if (running_) {
      // Reset kernel-level exclusion table.
      if (!inspector_->ioctl(0, PPM_IOCTL_EXCLUDE_NS_OF_PID, 0)) {
        CLOG(WARNING)
            << "Failed to reset the kernel-level PID namespace exclusion table via ioctl(): " << inspector_->getlasterr();
      }
    }
  }
}

void SysdigService::AddSignalHandler(std::unique_ptr<SignalHandler> signal_handler) {
  std::bitset<PPM_EVENT_MAX> event_filter;
  const auto& relevant_events = signal_handler->GetRelevantEvents();
  if (relevant_events.empty()) {
    event_filter.set();
  } else {
    const EventNames& event_names = EventNames::GetInstance();
    for (const auto& event_name : relevant_events) {
      for (ppm_event_type event_id : event_names.GetEventIDs(event_name)) {
        event_filter.set(event_id);
        global_event_filter_.set(event_id);
      }
    }
  }

  signal_handlers_.emplace_back(std::move(signal_handler), event_filter);
}

}  // namespace collector
