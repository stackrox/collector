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
#include "SysdigService.h"
#include "Utility.h"

extern unsigned char g_bpf_drop_syscalls[];
extern const struct syscall_evt_pair g_syscall_table[];  // defined in libscap

namespace collector {
class IKernelDriver {
 public:
  virtual bool Setup(const CollectorConfig& config, sinsp& inspector) = 0;
};

class KernelDriverModule : public IKernelDriver {
 public:
  KernelDriverModule() = default;

  bool Setup(const CollectorConfig& config, sinsp& inspector) override {
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

    if (!insert(config.Syscalls(), SysdigService::kModulePath)) {
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

    /* Get only necessary tracepoints. */
    auto tp_set = libsinsp::events::enforce_simple_tp_set();
    std::unordered_set<ppm_sc_code> ppm_sc;

    try {
      inspector.open_kmod(DEFAULT_DRIVER_BUFFER_BYTES_DIM, ppm_sc, tp_set);
    } catch (const sinsp_exception& ex) {
      CLOG(WARNING) << ex.what();
      return false;
    }

    return true;
  }

 private:
  bool insert(const std::vector<std::string>& syscalls, std::string path);
};

class KernelDriverEBPF : public IKernelDriver {
 public:
  KernelDriverEBPF() = default;

  bool Setup(const CollectorConfig& config, sinsp& inspector) override {
    FDHandle fd = FDHandle(open(SysdigService::kProbePath, O_RDONLY));
    if (!fd.valid()) {
      CLOG(ERROR) << "Cannot open eBPF probe at " << SysdigService::kProbePath;
      return false;
    }
    setDropSyscalls(config.Syscalls());

    /* Get only necessary tracepoints. */
    auto tp_set = libsinsp::events::enforce_simple_tp_set();
    std::unordered_set<ppm_sc_code> ppm_sc;

    try {
      inspector.open_bpf(SysdigService::kProbePath, DEFAULT_DRIVER_BUFFER_BYTES_DIM, ppm_sc, tp_set);
    } catch (const sinsp_exception& ex) {
      CLOG(WARNING) << ex.what();
      return false;
    }

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

class KernelDriverCOREEBPF : public IKernelDriver {
 public:
  KernelDriverCOREEBPF() = default;

  bool Setup(const CollectorConfig& config, sinsp& inspector) override {
    /* Capture only necessary tracepoints and syscalls. */
    auto tp_set = libsinsp::events::enforce_simple_tp_set();
    std::unordered_set<ppm_sc_code> ppm_sc;

    /*
     * Convert text reprecentation of event type into an actual syscall code
     * using g_syscall_table.
     */
    const EventNames& event_names = EventNames::GetInstance();
    for (const auto& syscall_str : config.Syscalls()) {
      for (ppm_event_type event_id : event_names.GetEventIDs(syscall_str)) {
        uint16_t syscall_id = event_names.GetEventSyscallID(event_id);
        if (!syscall_id) {
          continue;
        }

        syscall_evt_pair syscall = g_syscall_table[syscall_id];
        ppm_sc.insert((ppm_sc_code)syscall.ppm_sc);
      }
    }

    try {
      inspector.open_modern_bpf(DEFAULT_DRIVER_BUFFER_BYTES_DIM,
                                DEFAULT_CPU_FOR_EACH_BUFFER,
                                true, ppm_sc, tp_set);
    } catch (const sinsp_exception& ex) {
      CLOG(WARNING) << ex.what();
      return false;
    }

    return true;
  }
};
}  // namespace collector

#endif
