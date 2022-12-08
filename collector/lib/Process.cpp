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

#include "Process.h"

#include <chrono>

#include "CollectorStats.h"
#include "SysdigService.h"

namespace collector {

const std::string Process::NOT_AVAILABLE("N/A");

ProcessStore::ProcessStore(SysdigService* falco_instance) : falco_instance_(falco_instance) {
  cache_ = std::make_shared<std::unordered_map<uint64_t, std::weak_ptr<Process>>>();
}

const std::shared_ptr<Process> ProcessStore::Fetch(uint64_t pid) {
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
      falco_callback_(
          new std::function<void(std::shared_ptr<sinsp_threadinfo>)>(
              std::bind(&Process::ProcessInfoResolved, this, std::placeholders::_1))) {
  falco_instance->GetProcessInformation(pid, falco_callback_);
}

Process::~Process() {
  cache_->erase(pid_);
}

void Process::ProcessInfoResolved(std::shared_ptr<sinsp_threadinfo> process_info) {
  std::unique_lock<std::mutex> lock(process_info_mutex_);

  if (process_info) {
    CLOG(DEBUG) << "Process-info resolved. PID: " << pid() << " Exe: " + process_info->get_exe();
  } else {
    CLOG(WARNING) << "Process-info request failed. PID: " << pid();
  }

  falco_threadinfo_ = process_info;
  process_info_resolved_ = true;

  process_info_condition_.notify_all();
}

void Process::WaitForProcessInfo() const {
  std::unique_lock<std::mutex> lock(process_info_mutex_);

  COUNTER_ADD(
      process_info_resolved_ ? CollectorStats::process_info_hit : CollectorStats::process_info_miss,
      1);

  if (!process_info_resolved_) {
    std::cv_status status;

    status = process_info_condition_.wait_for(lock, std::chrono::seconds(30));

    CLOG_IF(std::cv_status::timeout == status, ERROR) << "Timed-out waiting for process-info. PID: " << pid();
  }
}

std::ostream& operator<<(std::ostream& os, const Process& process) {
  std::string processString = "ContainerID: " + process.container_id() + " Exe: " + process.exe() + " ExePath: ";
  processString += process.exe_path() + " Args: " + process.args() + " PID: " + std::to_string(process.pid());
  return os << processString;
}

}  // namespace collector
