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

#include <cstdint>
#include <memory>
#include <unordered_map>

namespace collector {

// A process
class Process {
 public:
  Process(const std::string& container_id,
          const std::string& comm,
          const std::string& exe,
          const std::string& exe_path,
          const std::string& args, uint64_t pid)
      : container_id_(container_id),
        comm_(comm),
        exe_(exe),
        exe_path_(exe_path),
        args_(args),
        pid_(pid) {}
  virtual ~Process() {}

  const std::string& getContainerId() const { return container_id_; }
  const std::string& getComm() const { return comm_; }
  const std::string& getExe() const { return exe_; }
  const std::string& getExePath() const { return exe_path_; }
  const std::string& getArgs() const { return args_; }
  uint64_t getPid() const { return pid_; }

  bool operator==(Process& other) {
    return pid_ == other.pid_;
  }

 protected:
  std::string container_id_;
  std::string comm_;      // binary name
  std::string exe_;       // argv[0]
  std::string exe_path_;  // full binary path
  std::string args_;      // space separated concatenation of arguments
  uint64_t pid_;
};

/* A Process object store used to deduplicate process information.
   Processes are kept in the store as long as they are referenced from the outside.
   References are created by FetchReference, which will create the Process object
   if necessary as a side-effect. */
class ProcessStore {
 public:
  static ProcessStore global_instance;

  /* Get a reference to a matching Process stored in the cache.
     The Process is created if it does not exist.
     This method always returns a valid Process reference. */
  const std::shared_ptr<Process> FetchReference(const Process& p);

  // Get a Process by PID if it exists in the store, null otherwise.
  const std::shared_ptr<Process> FindByPid(uint64_t pid) const;

 private:
  class CachedProcess : public Process {
   public:
    CachedProcess(const Process& p, ProcessStore& store) : Process(p), store_(store) {}
    ~CachedProcess();

   private:
    ProcessStore& store_;
  };

  std::unordered_map<uint64_t, std::weak_ptr<CachedProcess>> cache_;
};

}  // namespace collector

#endif