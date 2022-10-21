#ifndef COLLECTOR_KERNEL_DRIVER_H
#define COLLECTOR_KERNEL_DRIVER_H

#include <string>

extern "C" {
#include <cap-ng.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
}

#include "CollectorConfig.h"
#include "EventNames.h"
#include "FileSystem.h"
#include "Logging.h"
#include "Utility.h"

extern unsigned char g_bpf_drop_syscalls[];

namespace collector {

class IKernelDriver {
 public:
  virtual bool Setup(const CollectorConfig& config, std::string path) = 0;
};

class KernelDriverModule : public IKernelDriver {
 public:
  bool Setup(const CollectorConfig& config, std::string path) override {
    // First action: drop all capabilities except for:
    // SYS_MODULE (inserting the module),
    // SYS_PTRACE (reading from /proc),
    // DAC_OVERRIDE (opening the device files with
    //               O_RDWR regardless of actual permissions).
    capng_clear(CAPNG_SELECT_BOTH);
    capng_updatev(
        CAPNG_ADD,
        static_cast<capng_type_t>(CAPNG_EFFECTIVE | CAPNG_PERMITTED),
        CAP_SYS_MODULE,
        CAP_DAC_OVERRIDE,
        CAP_SYS_PTRACE,
        -1);

    if (capng_apply(CAPNG_SELECT_BOTH) != 0) {
      CLOG(WARNING) << "Failed to drop capabilities: " << StrError();
    }

    if (!insert(config.Syscalls(), path)) {
      CLOG(ERROR) << "Failed to insert kernel module";
      return false;
    }

    // if we've successfully inserted, drop SYS_MODULE capability
    capng_updatev(
        CAPNG_DROP,
        static_cast<capng_type_t>(CAPNG_EFFECTIVE | CAPNG_PERMITTED),
        CAP_SYS_MODULE,
        -1);

    if (capng_apply(CAPNG_SELECT_BOTH) != 0) {
      // not a fatal error, as we can continue, but needs to be reported.
      CLOG(WARNING) << "Failed to drop SYS_MODULE capability: " << StrError();
    }

    return true;
  }

 private:
  bool insert(const std::vector<std::string>& syscalls, std::string path);
};

class KernelDriverEBPF : public IKernelDriver {
 public:
  bool Setup(const CollectorConfig& config, std::string path) override {
    FDHandle fd = FDHandle(open(path.c_str(), O_RDONLY));
    if (!fd.valid()) {
      CLOG(ERROR) << "Cannot open eBPF probe at " << path;
      return false;
    }
    setDropSyscalls(config.Syscalls());
    return true;
  }

 private:
  void setDropSyscalls(const std::vector<std::string>& syscalls) {
    // Initialize bpf syscall drop table to drop all
    for (int i = 0; i < SYSCALL_TABLE_SIZE; i++) {
      g_bpf_drop_syscalls[i] = 1;
    }
    // Do not drop syscalls from given list
    const EventNames& event_names = EventNames::GetInstance();
    for (const auto& syscall_str : syscalls) {
      for (ppm_event_type event_id : event_names.GetEventIDs(syscall_str)) {
        uint16_t syscall_id = event_names.GetEventSyscallID(event_id);
        if (!syscall_id) {
          continue;
        }
        g_bpf_drop_syscalls[syscall_id] = 0;
      }
    }
  }
};

}  // namespace collector

#endif
