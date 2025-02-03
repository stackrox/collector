#include "HostHeuristics.h"

#include "Logging.h"
#include "host/src/lib.rs.h"

namespace collector {

namespace {

static const char* g_switch_collection_hint = "HINT: You may alternatively want to disable collection with collector.collectionMethod=NO_COLLECTION";

using HostInfo = ::rust::Box<collector::rust::HostInfo>;

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
    if (!host->has_ebpf_support()) {
      CLOG(FATAL) << host->distro().c_str() << " " << host->kernel_version()->release().c_str()
                  << " does not support eBPF, which is a requirement for Collector.";
    }

    auto kernel = host->kernel_version();
    // If we're configured to use eBPF with BTF, we try to be conservative
    // and fail instead of falling-back to ebpf.
    if (config.GetCollectionMethod() == CollectionMethod::CORE_BPF) {
      if (!host->has_btf_symbols()) {
        CLOG(FATAL) << "Missing BTF symbols, core_bpf is not available. "
                    << "They can be provided by the kernel when configured with DEBUG_INFO_BTF, "
                    << "or as file. "
                    << g_switch_collection_hint;
      }

      if (!host->has_bpf_ringbuf_support()) {
        CLOG(FATAL) << "Missing RingBuffer support, core_bpf is not available. "
                    << g_switch_collection_hint;
      }

      if (!host->has_bpf_tracing_support()) {
        CLOG(FATAL) << "Missing BPF tracepoint support.";
      }
    }

    // If configured to use regular eBPF, still verify if CORE_BPF is supported.
    if (config.GetCollectionMethod() == CollectionMethod::EBPF) {
      if (host->has_btf_symbols() &&
          host->has_bpf_ringbuf_support() &&
          host->has_bpf_tracing_support() &&
          kernel->machine() != "ppc64le") {
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
    if (host->is_docker_desktop()) {
      CLOG(FATAL) << host->distro().c_str() << " does not support eBPF.";
    }
  }
};

class PowerHeuristic : public Heuristic {
 public:
  void Process(HostInfo& host, const CollectorConfig& config, HostConfig* hconfig) const {
    auto k = host->kernel_version();

    if (k->machine() != "ppc64le") {
      return;
    }

    if (k->kernel() == 4 && k->major() == 18 && k->build_id() < 477) {
      CLOG(FATAL) << "RHEL 8.6 (kernel < 4.18.0-477) on ppc64le does not support CORE_BPF";
    }
  }
};

class CPUHeuristic : public Heuristic {
 public:
  // Enrich HostConfig with the number of possible CPU cores.
  void Process(HostInfo& host, const CollectorConfig& config, HostConfig* hconfig) const {
    hconfig->SetNumPossibleCPUs(host->num_possible_cpu());
  }
};

const std::unique_ptr<Heuristic> g_host_heuristics[] = {
    std::unique_ptr<Heuristic>(new CollectionHeuristic),
    std::unique_ptr<Heuristic>(new DockerDesktopHeuristic),
    std::unique_ptr<Heuristic>(new PowerHeuristic),
    std::unique_ptr<Heuristic>(new CPUHeuristic),
};

}  // namespace

HostConfig ProcessHostHeuristics(const CollectorConfig& config) {
  HostInfo host_info = collector::rust::host_info();
  HostConfig host_config;
  for (auto& heuristic : g_host_heuristics) {
    heuristic->Process(host_info, config, &host_config);
  }
  return host_config;
}

}  // namespace collector
