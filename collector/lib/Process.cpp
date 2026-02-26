#include "Process.h"

#include <chrono>

#include <libsinsp/sinsp.h>

#include "CollectorStats.h"
#include "Utility.h"
#include "system-inspector/Service.h"

namespace collector {

const std::string Process::NOT_AVAILABLE("N/A");

ProcessStore::ProcessStore(system_inspector::Service* instance) : instance_(instance) {
  cache_ = std::make_shared<std::unordered_map<uint64_t, std::weak_ptr<Process>>>();
}

const std::shared_ptr<IProcess> ProcessStore::Fetch(uint64_t pid) {
  auto cached_process_pair_iter = cache_->find(pid);

  if (cached_process_pair_iter != cache_->end()) {
    return cached_process_pair_iter->second.lock();
  }

  std::shared_ptr<Process> cached_process = std::make_shared<Process>(pid, cache_, instance_);

  cache_->emplace(pid, cached_process);
  return cached_process;
}

std::string Process::container_id() const {
  WaitForProcessInfo();

  if (system_inspector_threadinfo_) {
    for (const auto& [subsys, cgroup_path] : system_inspector_threadinfo_->cgroups()) {
      if (auto id = ExtractContainerIDFromCgroup(cgroup_path)) {
        return std::string(*id);
      }
    }
  }

  return NOT_AVAILABLE;
}

std::string Process::comm() const {
  WaitForProcessInfo();

  if (system_inspector_threadinfo_) {
    return system_inspector_threadinfo_->get_comm();
  }

  return NOT_AVAILABLE;
}

std::string Process::exe() const {
  WaitForProcessInfo();

  if (system_inspector_threadinfo_) {
    return system_inspector_threadinfo_->get_exe();
  }

  return NOT_AVAILABLE;
}

std::string Process::exe_path() const {
  WaitForProcessInfo();

  if (system_inspector_threadinfo_) {
    return system_inspector_threadinfo_->get_exepath();
  }

  return NOT_AVAILABLE;
}

std::string Process::args() const {
  WaitForProcessInfo();

  if (!system_inspector_threadinfo_) {
    return NOT_AVAILABLE;
  }

  if (system_inspector_threadinfo_->m_args.empty()) {
    return "";
  }

  std::ostringstream args;
  for (auto it = system_inspector_threadinfo_->m_args.begin(); it != system_inspector_threadinfo_->m_args.end();) {
    args << *it++;
    if (it != system_inspector_threadinfo_->m_args.end()) {
      args << " ";
    }
  }
  return args.str();
}

Process::Process(
    uint64_t pid,
    ProcessStore::MapRef cache,
    system_inspector::Service* instance)
    : pid_(pid),
      cache_(cache),
      process_info_pending_resolution_(false),
      system_inspector_callback_(
          new std::function<void(std::shared_ptr<sinsp_threadinfo>)>(
              std::bind(&Process::ProcessInfoResolved, this, std::placeholders::_1))) {
  if (instance) {
    process_info_pending_resolution_ = true;
    instance->GetProcessInformation(pid, system_inspector_callback_);
  }
}

Process::~Process() {
  if (cache_) {
    cache_->erase(pid_);
  }
}

void Process::ProcessInfoResolved(std::shared_ptr<sinsp_threadinfo> process_info) {
  std::unique_lock<std::mutex> lock(process_info_mutex_);

  if (process_info) {
    CLOG(DEBUG) << "Process-info resolved. PID: " << pid() << " Exe: " + process_info->m_exe;
  } else {
    CLOG(WARNING) << "Process-info request failed. PID: " << pid();
  }

  system_inspector_threadinfo_ = process_info;
  process_info_pending_resolution_ = false;

  process_info_condition_.notify_all();
}

void Process::WaitForProcessInfo() const {
  std::unique_lock<std::mutex> lock(process_info_mutex_);

  COUNTER_ADD(
      process_info_pending_resolution_ ? CollectorStats::process_info_miss : CollectorStats::process_info_hit,
      1);

  if (process_info_pending_resolution_) {
    std::cv_status status;

    WITH_TIMER(CollectorStats::process_info_wait) {
      status = process_info_condition_.wait_for(lock, std::chrono::seconds(30));
    }

    CLOG_IF(std::cv_status::timeout == status, ERROR) << "Timed-out waiting for process-info. PID: " << pid();
  }
}

std::ostream& operator<<(std::ostream& os, const IProcess& process) {
  std::string processString = "ContainerID: " + process.container_id() + " Exe: " + process.exe() + " ExePath: ";
  processString += process.exe_path() + " Args: " + process.args() + " PID: " + std::to_string(process.pid());
  return os << processString;
}

}  // namespace collector
