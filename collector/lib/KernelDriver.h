#ifndef COLLECTOR_KERNEL_DRIVER_H
#define COLLECTOR_KERNEL_DRIVER_H

#include <string>

extern "C" {
#include <cap-ng.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
}

#include "libsinsp/sinsp.h"

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

class KernelDriverEBPF : public IKernelDriver {
 public:
  KernelDriverEBPF() = default;

  bool Setup(const CollectorConfig& config, sinsp& inspector) override {
    FDHandle fd = FDHandle(open(SysdigService::kProbePath, O_RDONLY));
    if (!fd.valid()) {
      CLOG(ERROR) << "Cannot open eBPF probe at " << SysdigService::kProbePath;
      return false;
    }

    /* Get only necessary tracepoints. */
    auto syscall_names = config.Syscalls();

    std::unordered_set<std::string> syscall_set(syscall_names.begin(), syscall_names.end());
    auto ppm_sc = libsinsp::events::sc_names_to_sc_set(syscall_set);
    ppm_sc.insert(PPM_SC_SCHED_PROCESS_EXIT);

    try {
      inspector.open_bpf(SysdigService::kProbePath, DEFAULT_DRIVER_BUFFER_BYTES_DIM, ppm_sc);
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
    // std::unordered_set<ppm_sc_code> ppm_sc;
    auto syscall_names = config.Syscalls();

    std::unordered_set<std::string> syscall_set(syscall_names.begin(), syscall_names.end());
    auto ppm_sc = libsinsp::events::sc_names_to_sc_set(syscall_set);
    ppm_sc.insert(PPM_SC_SCHED_PROCESS_EXIT);

    try {
      inspector.open_modern_bpf(DEFAULT_DRIVER_BUFFER_BYTES_DIM,
                                DEFAULT_CPU_FOR_EACH_BUFFER,
                                true, ppm_sc);
    } catch (const sinsp_exception& ex) {
      if (config.CoReBPFHardfail()) {
        throw ex;
      } else {
        CLOG(WARNING) << ex.what();
        return false;
      }
    }

    return true;
  }
};
}  // namespace collector

#endif
