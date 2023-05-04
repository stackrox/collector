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

#include "HostHeuristics.h"

#include "Logging.h"

namespace collector {

namespace {

class Heuristic {
 public:
  // Process the given HostInfo and CollectorConfig to adjust HostConfig as necessary.
  // It is intended that any number of Heuristics may be applied to the configs,
  // to allow overriding of specific configuration options based on the platform.
  // Note: non-const reference to HostInfo due to its lazy-initialization.
  virtual void Process(HostInfo& host, const CollectorConfig& config, HostConfig* hconfig) const {}
};

class CollectionHeuristic : public Heuristic {
  void Process(HostInfo& host, const CollectorConfig& config, HostConfig* hconfig) const {
    // If we're configured to use eBPF with BTF, we try to be conservative
    // and fail instead of falling-back to other methods.
    if (config.GetCollectionMethod() == CORE_BPF) {
      if (!host.HasEBPFSupport()) {
        CLOG(FATAL) << host.GetDistro() << " " << host.GetKernelVersion().release
                    << " does not support eBPF, which is a requirement for core_bpf collection. "
                    << "HINT: You may want to use kernel module based collection "
                    << "with collector.collectionMethod=KERNEL_MODULE.";
      }

      if (!host.HasBTFSymbols()) {
        CLOG(FATAL) << "Missing BTF symbols, core_bpf is not available. "
                    << "They can be provided by the kernel when configured with DEBUG_INFO_BTF, "
                    << "or as file. "
                    << "HINT: You may alternatively want to use eBPF based collection "
                    << "with collector.collectionMethod=EBPF.";
        ;
      }

      if (!host.HasBPFRingBufferSupport()) {
        CLOG(FATAL) << "Missing RingBuffer support, core_bpf is not available. "
                    << "HINT: You may alternatively want to use eBPF based collection "
                    << "with collector.collectionMethod=EBPF.";
      }

      if (!host.HasBPFTracingSupport()) {
        CLOG(FATAL) << "Missing BPF tracepoint support.";
      }
    }

    // If we're configured to use eBPF but the host we're running on
    // does not support it, we can try to use kernel modules instead.
    // The exception to this is COS, where third party modules are not
    // supported, so there is nothing we can do and must exit.
    if ((config.GetCollectionMethod() == EBPF) && !host.HasEBPFSupport()) {
      if (host.IsCOS()) {
        CLOG(FATAL) << host.GetDistro() << " does not support third-party kernel modules or the required eBPF features.";
      }

      CLOG(WARNING) << host.GetDistro() << " " << host.GetKernelVersion().release
                    << " does not support ebpf based collection.";
      CLOG(WARNING) << "Switching to kernel module based collection, please set "
                    << "collector.collectionMethod=KERNEL_MODULE to remove this message";
      hconfig->SetCollectionMethod(KERNEL_MODULE);
    }
  }
};

class CosHeuristic : public Heuristic {
 public:
  // This is additional to the collection method heuristic above.
  // If we're on COS, and configured to use kernel modules, we attempt
  // to switch to eBPF collection if possible, otherwise we are unable
  // to collect and must exit.
  void Process(HostInfo& host, const CollectorConfig& config, HostConfig* hconfig) const {
    if (!host.IsCOS()) {
      return;
    }

    if (!config.UseEbpf()) {
      CLOG(ERROR) << host.GetDistro() << " does not support third-party kernel modules";
      if (host.HasEBPFSupport() && !config.ForceKernelModules()) {
        CLOG(WARNING) << "Switching to eBPF based collection, please set "
                      << "collector.collectionMethod=EBPF to remove this message";
        hconfig->SetCollectionMethod(EBPF);
      } else {
        CLOG(FATAL) << "Unable to switch to eBPF collection on " << host.GetDistro();
      }
    }
  }
};

class DockerDesktopHeuristic : public Heuristic {
 public:
  // Docker Desktop does not support eBPF so we switch to use kernel
  // modules instead.
  void Process(HostInfo& host, const CollectorConfig& config, HostConfig* hconfig) const {
    if (!host.IsDockerDesktop()) {
      return;
    }

    if (config.UseEbpf()) {
      CLOG(WARNING) << host.GetDistro() << " does not support eBPF, switching to kernel module based collection.";
      hconfig->SetCollectionMethod(KERNEL_MODULE);
    }
  }
};

class MinikubeHeuristic : public Heuristic {
 public:
  // This is additional to the collection method heuristic above.
  // If we're on Minikube, and configured to use kernel modules, we attempt
  // to switch to eBPF collection if possible, otherwise we are unable
  // to collect and must exit.
  void Process(HostInfo& host, const CollectorConfig& config, HostConfig* hconfig) const {
    if (!host.IsMinikube()) {
      return;
    }

    if (!config.UseEbpf()) {
      CLOG(WARNING) << host.GetHostname() << " does not support third-party kernel modules";
      CLOG(WARNING) << "Switching to eBPF based collection, please set collector.collectionMethod=EBPF to remove this message";
      hconfig->SetCollectionMethod(EBPF);
    }
  }
};

class SecureBootHeuristic : public Heuristic {
 public:
  // If the system is loaded in UEFI mode with Secure Boot feature enabled,
  // the kernel does not permit the insertion of unsigned kernel modules.
  // In this case switch to eBPF if configured to use kernel modules.
  void Process(HostInfo& host, const CollectorConfig& config, HostConfig* hconfig) const {
    SecureBootStatus sb_status;

    // No switching from kernel modules to eBPF necessary if already use eBPF,
    // or asked to force kernel modules, or not booted with UEFI. The latter
    // means legacy BIOS mode, where no Secure Boot available.
    if (config.UseEbpf() || config.ForceKernelModules() || !host.IsUEFI()) {
      return;
    }

    // Switch to eBPF in case there is a chance Secure Boot is on
    sb_status = host.GetSecureBootStatus();
    if (sb_status == SecureBootStatus::ENABLED) {
      CLOG(WARNING) << "SecureBoot is enabled preventing unsigned third-party "
                    << "kernel modules. Switching to eBPF based collection.";
      hconfig->SetCollectionMethod(EBPF);
    }

#ifdef __x86_64__
    if (sb_status == SecureBootStatus::NOT_DETERMINED) {
      CLOG(WARNING) << "SecureBoot status could not be determined. "
                    << "Switching to eBPF based collection.";
      hconfig->SetCollectionMethod(EBPF);
    }
#endif
  }
};

class GardenLinuxHeuristic : public Heuristic {
 public:
  // Garden Linux stopped accepting kernel modules without a signature in
  // version 576.3, anything newer than that will be configured to use eBPF.
  void Process(HostInfo& host, const CollectorConfig& config, HostConfig* hconfig) const {
    if (!host.IsGarden() || config.UseEbpf()) {
      return;
    }

    const std::string& distro = host.GetDistro();
    std::string::size_type minor_version_separator = distro.rfind(".");

    if (minor_version_separator == std::string::npos) {
      CLOG(WARNING) << "Failed to find minor separator - " << distro;
      return;
    }

    std::string::size_type major_version_separator = distro.rfind(" ");

    if (major_version_separator == std::string::npos) {
      CLOG(WARNING) << "Failed to find major separator - " << distro;
      return;
    }

    auto major_version = std::stoul(distro.substr(major_version_separator + 1, minor_version_separator));
    auto minor_version = std::stoul(distro.substr(minor_version_separator + 1));

    if (major_version > 576 || (major_version == 576 && minor_version >= 3)) {
      CLOG(WARNING) << distro << " does not support kernel module based collection. "
                    << "Switching to eBPF based collection, set "
                    << "collector.collectionMethod=EBPF to remove this message";
      hconfig->SetCollectionMethod(EBPF);
      return;
    }
  }
};

const std::unique_ptr<Heuristic> g_host_heuristics[] = {
    std::unique_ptr<Heuristic>(new CollectionHeuristic),
    std::unique_ptr<Heuristic>(new CosHeuristic),
    std::unique_ptr<Heuristic>(new DockerDesktopHeuristic),
    std::unique_ptr<Heuristic>(new MinikubeHeuristic),
    std::unique_ptr<Heuristic>(new SecureBootHeuristic),
    std::unique_ptr<Heuristic>(new GardenLinuxHeuristic),
};

}  // namespace

HostConfig ProcessHostHeuristics(const CollectorConfig& config) {
  HostInfo& host_info = HostInfo::Instance();
  HostConfig host_config;
  for (auto& heuristic : g_host_heuristics) {
    heuristic->Process(host_info, config, &host_config);
  }
  return host_config;
}

}  // namespace collector
