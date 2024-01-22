#include "Process.h"

#include <chrono>

#include "CollectorStats.h"
#include "SysdigService.h"

namespace collector {

const std::string Process::NOT_AVAILABLE("N/A");

ProcessStore::ProcessStore(SysdigService* falco_instance) : falco_instance_(falco_instance) {
  cache_ = std::make_shared<std::unordered_map<uint64_t, std::weak_ptr<Process>>>();
}

const std::shared_ptr<IProcess> ProcessStore::Fetch(uint64_t pid) {
  auto cached_process_pair_iter = cache_->find(pid);

  if (cached_process_pair_iter != cache_->end()) {
    return cached_process_pair_iter->second.lock();
  }

  std::shared_ptr<Process> cached_process = std::make_shared<Process>(pid, cache_, falco_instance_);

  cache_->emplace(pid, cached_process);
  return cached_process;
}

std::string Process::container_id() const {
  WaitForProcessInfo();

  if (falco_threadinfo_) {
    return falco_threadinfo_->m_container_id;
  }

  return NOT_AVAILABLE;
}

std::string Process::comm() const {
  WaitForProcessInfo();

  if (falco_threadinfo_) {
    return falco_threadinfo_->get_comm();
  }

  return NOT_AVAILABLE;
}

std::string Process::exe() const {
  WaitForProcessInfo();

  if (falco_threadinfo_) {
    return falco_threadinfo_->get_exe();
  }

  return NOT_AVAILABLE;
}

std::string Process::exe_path() const {
  WaitForProcessInfo();

  if (falco_threadinfo_) {
    return falco_threadinfo_->get_exepath();
  }

  return NOT_AVAILABLE;
}

std::string Process::args() const {
  WaitForProcessInfo();

  if (!falco_threadinfo_) {
    return NOT_AVAILABLE;
  }

  if (falco_threadinfo_->m_args.empty()) {
    return "";
  }

  std::ostringstream args;
  for (auto it = falco_threadinfo_->m_args.begin(); it != falco_threadinfo_->m_args.end();) {
    args << *it++;
    if (it != falco_threadinfo_->m_args.end()) args << " ";
  }
  return args.str();
}

Process::Process(
    uint64_t pid,
    ProcessStore::MapRef cache,
    SysdigService* falco_instance)
    : pid_(pid),
      cache_(cache),
      process_info_pending_resolution_(false),
      falco_callback_(
          new std::function<void(std::shared_ptr<sinsp_threadinfo>)>(
              std::bind(&Process::ProcessInfoResolved, this, std::placeholders::_1))) {
  if (falco_instance) {
    process_info_pending_resolution_ = true;
    falco_instance->GetProcessInformation(pid, falco_callback_);
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

  falco_threadinfo_ = process_info;
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
