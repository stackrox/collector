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
                    << " does not support eBPF, which is a requirement for core_bpf collection.";
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
      CLOG(FATAL) << host.GetDistro() << " " << host.GetKernelVersion().release
                  << " does not support ebpf based collection.";
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
