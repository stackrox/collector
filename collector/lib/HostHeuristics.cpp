#include "HostHeuristics.h"

#include "Logging.h"

namespace collector {

namespace {

class CollectionHeuristic : public Heuristic {
  // If we're configured to use eBPF but the host we're running on
  // does not support it, we can try to use kernel modules instead.
  // The exception to this is COS, where third party modules are not
  // supported, so there is nothing we can do and must exit.
  void Process(HostInfo& host, CollectorConfig* config) {
    if (config->UseEbpf() && !host.HasEBPFSupport()) {
      if (host.IsCOS()) {
        CLOG(FATAL) << host.GetDistro() << " does not support third-party kernel modules or the required eBPF features.";
      }

      CLOG(ERROR) << host.GetDistro() << " " << host.GetKernelVersion().release
                  << " does not support ebpf based collection.";
      CLOG(WARNING) << "Switching to kernel module based collection, please configure RUNTIME_SUPPORT=kernel-module";
      // TODO: config->SetCollectionMethod("kernel-module");
    }
  }
};

class CosHeuristic : public Heuristic {
 public:
  // This is additional to the collection method heuristic above.
  // If we're on COS, and configured to use kernel modules, we attempt
  // to switch to eBPF collection if possible, otherwise we are unable
  // to collect and must exit.
  void Process(HostInfo& host, CollectorConfig* config) {
    if (!host.IsCOS()) {
      return;
    }

    if (!config->UseEbpf()) {
      CLOG(ERROR) << host.GetDistro() << " does not support third-party kernel modules";
    }

    if (host.HasEBPFSupport()) {
      CLOG(WARNING) << "switching to eBPF based collection, please configure RUNTIME_SUPPORT=ebpf";
      // TODO: config->SetCollectionMethod("ebpf");
    } else {
      CLOG(FATAL) << "unable to switch to eBPF collection on this host";
    }
  }
};

class DockerDesktopHeuristic : public Heuristic {
 public:
  // Docker Desktop does not support eBPF so we switch to use kernel
  // modules instead.
  void Process(HostInfo& host, CollectorConfig* config) {
    if (!host.IsDockerDesktop()) {
      return;
    }

    if (config->UseEbpf()) {
      CLOG(WARNING) << host.GetDistro() << " does not support eBPF, switching to kernel module based collection.";
      // TODO: config->SetCollectionMethod("kernel-module");
    }
  }
};

const CollectionHeuristic kCollectionHeuristic;
const CosHeuristic kCosHeuristic;
const DockerDesktopHeuristic kDockerDesktopHeuristic;

}  // namespace

const std::vector<Heuristic*> g_host_heuristics = {
    (Heuristic*)&kCollectionHeuristic,
    (Heuristic*)&kCosHeuristic,
    (Heuristic*)&kDockerDesktopHeuristic,
};

}  // namespace collector