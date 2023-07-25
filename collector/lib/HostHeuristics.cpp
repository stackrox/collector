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

class S390XHeuristic : public Heuristic {
 public:
  // S390X does not support eBPF ealier than 4.18.0-348 (rhel8.5) so we switch to use corebpf
  // instead.
  void Process(HostInfo& host, const CollectorConfig& config, HostConfig* hconfig) const {
    auto k = host.GetKernelVersion();
    std::string os_id = host.GetOSID();

    if (k.machine != "s390x") {
      return;
    }

    if (os_id == "rhel" || os_id == "centos") {
      // example release version: 4.18.0-305.88.1.el8_4.s390x
      // build_id = 305
      if (k.release.find(".el8.") != std::string::npos) {
        if (k.kernel == 4 && k.major == 18 && k.minor == 0) {
          // rhel 8.4 release according to https://access.redhat.com/articles/3078#RHEL8
          // rhel 8.3 and earlier never supported on s390x
          if (k.build_id >= 305 && k.build_id < 348) {
            CLOG(WARNING) << "RHEL 8.4 on s390x does not support eBPF, switching to CO.RE eBPF module based collection.";
            hconfig->SetCollectionMethod(CollectionMethod::CORE_BPF);
          }
        }
      }
    }
  }
};

const std::unique_ptr<Heuristic> g_host_heuristics[] = {
    std::unique_ptr<Heuristic>(new CollectionHeuristic),
    std::unique_ptr<Heuristic>(new DockerDesktopHeuristic),
    std::unique_ptr<Heuristic>(new S390XHeuristic),
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
