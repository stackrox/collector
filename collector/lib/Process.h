#pragma once

#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include "libsinsp/sinsp.h"

#include "system-inspector/EventExtractor.h"

// forward declarations
class sinsp_threadinfo;
namespace collector {
class IProcess;
class Process;
class Service;
}  // namespace collector

namespace collector::system_inspector {
class Service;
}

namespace collector {
/* A Process object store used to deduplicate process information.
   Processes are kept in the store as long as they are referenced from the outside.
   When a process cannot be found in the store, it is fetched as a side-effect. */
class ProcessStore {
 public:
  /* system-inspector is the source of process information */
  ProcessStore(system_inspector::Service* instance);

  /* Get a Process by PID.
     Returns a reference to the cached Process entry, which may have just been created
     if it wasn't already known. */
  const std::shared_ptr<IProcess> Fetch(uint64_t pid);

  typedef std::shared_ptr<std::unordered_map<uint64_t, std::weak_ptr<Process>>> MapRef;

 private:
  system_inspector::Service* instance_;
  MapRef cache_;
};

class IProcess {
 public:
  virtual uint64_t pid() const = 0;
  virtual std::string name() const = 0;
  virtual std::string container_id() const = 0;
  virtual std::string comm() const = 0;
  virtual std::string exe() const = 0;
  virtual std::string exe_path() const = 0;
  virtual std::string args() const = 0;

  virtual bool operator==(IProcess& other) {
    return pid() == other.pid();
  }

  virtual ~IProcess() {}
};

// Information collected about a process.
class Process : public IProcess {
 public:
  inline uint64_t pid() const override { return pid_; }
  std::string name() const override;
  std::string container_id() const override;
  std::string comm() const override;
  std::string exe() const override;
  std::string exe_path() const override;
  std::string args() const override;

  /* - when 'cache' is provided, this process will remove itself from it upon deletion.
   * - 'instance' is used to request the process information from the system. */
  Process(uint64_t pid, ProcessStore::MapRef cache = 0, system_inspector::Service* instance = 0);

  ~Process();

 private:
  static const std::string NOT_AVAILABLE;  // = "N/A"

  uint64_t pid_;
  // A cache we are referenced from. Remove ourselves upon deletion.
  ProcessStore::MapRef cache_;

  bool process_info_pending_resolution_;
  mutable std::mutex process_info_mutex_;
  mutable std::condition_variable process_info_condition_;

  // Underlying thread info provided asynchronously by system-inspector via system_inspector_callback_
  mutable std::shared_ptr<sinsp_threadinfo> system_inspector_threadinfo_;
  // use a shared pointer here to handle deletion while the callback is pending
  std::shared_ptr<std::function<void(std::shared_ptr<sinsp_threadinfo>)>> system_inspector_callback_;

  // entry-point when system inspector resolved the requested process info
  void ProcessInfoResolved(std::shared_ptr<sinsp_threadinfo> process_info);

  // block until process information is available, or timeout
  void WaitForProcessInfo() const;
};

std::ostream& operator<<(std::ostream& os, const IProcess& process);

}  // namespace collector
