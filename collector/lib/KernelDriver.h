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
#include "common/FileSystem.h"
#include "common/Logging.h"
#include "common/Utility.h"
#include "system-inspector/EventNames.h"
#include "system-inspector/Service.h"

extern const struct syscall_evt_pair g_syscall_table[];  // defined in libscap
static const unsigned long DRIVER_BUFFER_DIM = 16UL * 1024UL * 1024UL;

namespace collector {
class IKernelDriver {
 public:
  virtual bool Setup(const CollectorConfig& config, sinsp& inspector) = 0;

  /*
   * Convert text representation of event type into an actual syscall code
   * using g_syscall_table.
   */
  std::unordered_set<ppm_sc_code> GetSyscallList(const CollectorConfig& config) {
    std::unordered_set<ppm_sc_code> ppm_sc;
    const EventNames& event_names = EventNames::GetInstance();

    for (const auto& syscall_str : config.Syscalls()) {
      for (ppm_event_code event_id : event_names.GetEventIDs(syscall_str)) {
        uint16_t syscall_id = event_names.GetEventSyscallID(event_id);
        if (!syscall_id) {
          continue;
        }

        syscall_evt_pair syscall = g_syscall_table[syscall_id];
        ppm_sc.insert((ppm_sc_code)syscall.ppm_sc);
      }
    }

    /*
     * Earlier version of Falco used to include procexit and sched_switch by
     * default, now we have to explicitly add it alongside with the required
     * syscalls. procexit is essential for keeping threadinfo cache under
     * control, and sched_switch makes conveying process information more
     * reliable.
     */
    ppm_sc.insert((ppm_sc_code)PPM_SC_SCHED_PROCESS_EXIT);
    ppm_sc.insert((ppm_sc_code)PPM_SC_SCHED_SWITCH);
    return ppm_sc;
  }
};

class KernelDriverEBPF : public IKernelDriver {
 public:
  KernelDriverEBPF() = default;

  bool Setup(const CollectorConfig& config, sinsp& inspector) override {
    FDHandle fd = FDHandle(open(system_inspector::Service::kProbePath, O_RDONLY));
    if (!fd.valid()) {
      CLOG(ERROR) << "Cannot open eBPF probe at " << system_inspector::Service::kProbePath;
      return false;
    }

    /* Get only necessary tracepoints. */
    std::unordered_set<ppm_sc_code> ppm_sc = GetSyscallList(config);

    try {
      inspector.open_bpf(system_inspector::Service::kProbePath,
                         config.GetSinspBufferSize(),
                         ppm_sc);
    } catch (const sinsp_exception& ex) {
      CLOG(WARNING) << ex.what();
      return false;
    }

    return true;
  }
};

class KernelDriverCOREEBPF : public IKernelDriver {
 public:
  KernelDriverCOREEBPF() = default;

  bool Setup(const CollectorConfig& config, sinsp& inspector) override {
    /* Capture only necessary tracepoints and syscalls. */
    std::unordered_set<ppm_sc_code> ppm_sc = GetSyscallList(config);

    try {
      inspector.open_modern_bpf(config.GetSinspBufferSize(),
                                config.GetSinspCpuPerBuffer(),
                                true, ppm_sc);
    } catch (const sinsp_exception& ex) {
      throw ex;
    }

    return true;
  }
};
}  // namespace collector

#endif
