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
    // All our probes depend on eBPF.
    if (!host.HasEBPFSupport()) {
      CLOG(FATAL) << host.GetDistro() << " " << host.GetKernelVersion().release
                  << " does not support eBPF, which is a requirement for Collector.";
    }

    auto kernel = host.GetKernelVersion();
    // If we're configured to use eBPF with BTF, we try to be conservative
    // and fail instead of falling-back to ebpf.
    if (config.GetCollectionMethod() == CollectionMethod::CORE_BPF) {
      if (kernel.machine == "ppc64le") {
        CLOG(FATAL) << "CORE_BPF collection method is not supported on ppc64le. "
                    << "HINT: Change collection method to eBPF with collector.collectionMethod=EBPF.";
      }

      if (!host.HasBTFSymbols()) {
        CLOG(FATAL) << "Missing BTF symbols, core_bpf is not available. "
                    << "They can be provided by the kernel when configured with DEBUG_INFO_BTF, "
                    << "or as file. "
                    << "HINT: You may alternatively want to use eBPF based collection "
                    << "with collector.collectionMethod=EBPF.";
      }

      if (!host.HasBPFRingBufferSupport()) {
        CLOG(FATAL) << "Missing RingBuffer support, core_bpf is not available. "
                    << "HINT: Change collection method to eBPF with collector.collectionMethod=EBPF.";
      }

      if (!host.HasBPFTracingSupport()) {
        CLOG(FATAL) << "Missing BPF tracepoint support.";
      }
    }

    // If configured to use regular eBPF, still verify if CORE_BPF is supported.
    if (config.GetCollectionMethod() == CollectionMethod::EBPF) {
      if (host.HasBTFSymbols() &&
          host.HasBPFRingBufferSupport() &&
          host.HasBPFTracingSupport() &&
          kernel.machine != "ppc64le") {
        CLOG(INFO) << "CORE_BPF collection method is available. "
                   << "Check the documentation to compare features of "
                   << "available collection methods.";
      }
    }
  }
};

class DockerDesktopHeuristic : public Heuristic {
 public:
  // Docker Desktop does not support eBPF so we don't support it.
  void Process(HostInfo& host, const CollectorConfig& config, HostConfig* hconfig) const {
    if (host.IsDockerDesktop()) {
      CLOG(FATAL) << host.GetDistro() << " does not support eBPF.";
    }
  }
};

class S390XHeuristic : public Heuristic {
 public:
  // S390X does not support eBPF ealier than rhel8.5 so we switch to use corebpf instead
  void Process(HostInfo& host, const CollectorConfig& config, HostConfig* hconfig) const {
    auto k = host.GetKernelVersion();
    std::string os_id = host.GetOSID();

    if (k.machine != "s390x" || config.GetCollectionMethod() == CollectionMethod::CORE_BPF) {
      return;
    }

    // example release version: 4.18.0-305.88.1.el8_4.s390x
    if (k.release.find(".el8_4.") != std::string::npos) {
      CLOG(WARNING) << "RHEL 8.4 on s390x does not support eBPF, switching to CO.RE eBPF module based collection.";
      hconfig->SetCollectionMethod(CollectionMethod::CORE_BPF);
    }
  }
};

class ARM64Heuristic : public Heuristic {
 public:
  void Process(HostInfo& host, const CollectorConfig& config, HostConfig* hconfig) const {
    auto kernel = host.GetKernelVersion();

    if (kernel.machine == "aarch64" && config.GetCollectionMethod() == CollectionMethod::EBPF) {
      CLOG(WARNING) << "eBPF collection method is not supported on ARM, switching to CO-RE BPF collection method.";
      hconfig->SetCollectionMethod(CollectionMethod::CORE_BPF);
    }
  }
};

const std::unique_ptr<Heuristic> g_host_heuristics[] = {
    std::unique_ptr<Heuristic>(new CollectionHeuristic),
    std::unique_ptr<Heuristic>(new DockerDesktopHeuristic),
    std::unique_ptr<Heuristic>(new S390XHeuristic),
    std::unique_ptr<Heuristic>(new ARM64Heuristic),
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
