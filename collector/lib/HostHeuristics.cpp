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

    // If we're configured to use eBPF with BTF, we try to be conservative
    // and fail instead of falling-back to ebpf.
    if (config.GetCollectionMethod() == CollectionMethod::CORE_BPF) {
      if (!host.HasBTFSymbols()) {
        CLOG(FATAL) << "Missing BTF symbols, core_bpf is not available. "
                    << "They can be provided by the kernel when configured with DEBUG_INFO_BTF, "
                    << "or as file. "
                    << "HINT: You may alternatively want to use eBPF based collection "
                    << "with collector.collectionMethod=EBPF.";
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

const std::unique_ptr<Heuristic> g_host_heuristics[] = {
    std::unique_ptr<Heuristic>(new CollectionHeuristic),
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
