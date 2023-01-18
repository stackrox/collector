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

#ifndef COLLECTOR_PROCESS_H
#define COLLECTOR_PROCESS_H

#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

// forward declarations
class sinsp_threadinfo;
namespace collector {
class Process;
class SysdigService;
}  // namespace collector

namespace collector {
/* A Process object store used to deduplicate process information.
   Processes are kept in the store as long as they are referenced from the outside.
   When a process cannot be found in the store, it is fetched as a side-effect. */
class ProcessStore {
 public:
  /* falco_instance is the source of process information */
  ProcessStore(SysdigService* falco_instance);

  /* Get a Process by PID.
     Returns a reference to the cached Process entry, which may have just been created
     if it wasn't already known. */
  const std::shared_ptr<Process> Fetch(uint64_t pid);

  typedef std::shared_ptr<std::unordered_map<uint64_t, std::weak_ptr<Process>>> MapRef;

 private:
  SysdigService* falco_instance_;
  MapRef cache_;
};

// Information collected about a process.
class Process {
 public:
  inline uint64_t pid() const { return pid_; }
  std::string container_id() const;
  std::string comm() const;
  std::string exe() const;
  std::string exe_path() const;
  std::string args() const;

  bool operator==(Process& other) {
    return container_id() == other.container_id() &&
           comm() == other.comm() &&
           exe_path() == other.exe_path() &&
           args() == other.args();
  }

  /* - when 'cache' is provided, this process will remove itself from it upon deletion.
   * - 'falco_instance' is used to request the process information from the system. */
  Process(uint64_t pid, ProcessStore::MapRef cache = 0, SysdigService* falco_instance = 0);
  ~Process();

 private:
  static const std::string NOT_AVAILABLE;  // = "N/A"

  uint64_t pid_;
  // A cache we are referenced from. Remove ourselves upon deletion.
  ProcessStore::MapRef cache_;

  bool process_info_pending_resolution_;
  mutable std::mutex process_info_mutex_;
  mutable std::condition_variable process_info_condition_;

  // Underlying thread info provided asynchronously by Falco via falco_callback_
  mutable std::shared_ptr<sinsp_threadinfo> falco_threadinfo_;
  // use a shared pointer here to handle deletion while the callback is pending
  std::shared_ptr<std::function<void(std::shared_ptr<sinsp_threadinfo>)>> falco_callback_;

  // entry-point when Falco resolved the requested process info
  void ProcessInfoResolved(std::shared_ptr<sinsp_threadinfo> process_info);

  // block until process information is available, or timeout
  void WaitForProcessInfo() const;
};

std::ostream& operator<<(std::ostream& os, const Process& process);

}  // namespace collector

#endif
