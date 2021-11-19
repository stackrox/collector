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
  // If we're configured to use eBPF but the host we're running on
  // does not support it, we can try to use kernel modules instead.
  // The exception to this is COS, where third party modules are not
  // supported, so there is nothing we can do and must exit.
  void Process(HostInfo& host, const CollectorConfig& config, HostConfig* hconfig) const {
    if (config.UseEbpf() && !host.HasEBPFSupport()) {
      if (host.IsCOS()) {
        CLOG(FATAL) << host.GetDistro() << " does not support third-party kernel modules or the required eBPF features.";
      }

      CLOG(ERROR) << host.GetDistro() << " " << host.GetKernelVersion().release
                  << " does not support ebpf based collection.";
      CLOG(WARNING) << "Switching to kernel module based collection, please set "
                    << "collector.collectionMethod=KERNEL_MODULE to remove this message";
      hconfig->SetCollectionMethod("kernel-module");
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
    }

    if (host.HasEBPFSupport()) {
      CLOG(WARNING) << "switching to eBPF based collection, please set "
                    << "collector.collectionMethod=EBPF to remove this message";
      hconfig->SetCollectionMethod("ebpf");
    } else {
      CLOG(FATAL) << "unable to switch to eBPF collection on this host";
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
      hconfig->SetCollectionMethod("kernel-module");
    }
  }
};

const std::unique_ptr<Heuristic> g_host_heuristics[] = {
    std::unique_ptr<Heuristic>(new CollectionHeuristic),
    std::unique_ptr<Heuristic>(new CosHeuristic),
    std::unique_ptr<Heuristic>(new DockerDesktopHeuristic),
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
