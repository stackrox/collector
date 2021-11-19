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

#include "CollectorConfig.h"

#include <sstream>

#include "CollectorArgs.h"
#include "EnvVar.h"
#include "HostHeuristics.h"
#include "HostInfo.h"
#include "Logging.h"
#include "Utility.h"

namespace collector {

namespace {

// If true, disable processing of network system call events and reading of connection information in /proc.
BoolEnvVar disable_network_flows("ROX_COLLECTOR_DISABLE_NETWORK_FLOWS", false);

// If true, retrieve tcp listening sockets while reading connection information in /proc.
BoolEnvVar ports_feature_flag("ROX_NETWORK_GRAPH_PORTS", true);

// If true, ignore connections with configured protocol and port pairs (e.g., udp/9).
BoolEnvVar network_drop_ignored("ROX_NETWORK_DROP_IGNORED", true);

// If true, attempt to download a kernel probe from within the collector binary.
BoolEnvVar alternate_probe_download("ROX_COLLECTOR_ALT_PROBE_DOWNLOAD", false);

// If true, set curl to be verbose, adding further logging that might be useful for debugging.
BoolEnvVar set_curl_verbose("ROX_COLLECTOR_SET_CURL_VERBOSE", false);

}  // namespace

constexpr bool CollectorConfig::kUseChiselCache;
constexpr bool CollectorConfig::kSnapLen;
constexpr bool CollectorConfig::kTurnOffScrape;
constexpr int CollectorConfig::kScrapeInterval;
constexpr char CollectorConfig::kCollectionMethod[];
constexpr char CollectorConfig::kChisel[];
constexpr const char* CollectorConfig::kSyscalls[];

const UnorderedSet<L4ProtoPortPair> CollectorConfig::kIgnoredL4ProtoPortPairs = {{L4Proto::UDP, 9}};
;

CollectorConfig::CollectorConfig(CollectorArgs* args) {
  // Set default configuration values
  use_chisel_cache_ = kUseChiselCache;
  scrape_interval_ = kScrapeInterval;
  turn_off_scrape_ = kTurnOffScrape;
  snap_len_ = kSnapLen;
  chisel_ = kChisel;
  collection_method_ = kCollectionMethod;

  for (const auto& syscall : kSyscalls) {
    syscalls_.push_back(syscall);
  }

  // Get hostname and path to host proc dir
  hostname_ = GetHostname();
  host_proc_ = GetHostPath("/proc");

  // Check user provided configuration
  if (args) {
    auto config = args->CollectorConfig();

    // Chisel Cache
    if (!config["useChiselCache"].empty()) {
      use_chisel_cache_ = config["useChiselCache"].asBool();
      CLOG(INFO) << "User configured useChiselCache=" << use_chisel_cache_;
    }

    // Scrape Interval
    if (!config["scrapeInterval"].empty()) {
      scrape_interval_ = std::stoi(config["scrapeInterval"].asString());
      CLOG(INFO) << "User configured scrapeInterval=" << scrape_interval_;
    }

    // Scrape Enabled/Disabled
    if (!config["turnOffScrape"].empty()) {
      turn_off_scrape_ = config["turnOffScrape"].asBool();
      CLOG(INFO) << "User configured turnOffScrape=" << turn_off_scrape_;
    }

    // Log Level
    if (!config["logLevel"].empty()) {
      logging::LogLevel level;
      if (logging::ParseLogLevelName(config["logLevel"].asString(), &level)) {
        logging::SetLogLevel(level);
        CLOG(INFO) << "User configured logLevel=" << config["logLevel"].asString();
      } else {
        CLOG(INFO) << "User configured logLevel is invalid " << config["logLevel"].asString();
      }
    }

    // Chisel
    if (args->Chisel().length()) {
      auto chiselB64 = args->Chisel();
      CLOG(INFO) << "User configured chisel=" << chiselB64;
      chisel_ = Base64Decode(chiselB64);
    }

    // Syscalls
    if (!config["syscalls"].empty() && config["syscalls"].isArray()) {
      auto syscall_list = config["syscalls"];
      std::vector<std::string> syscalls;
      for (const auto& syscall_json : syscall_list) {
        syscalls.push_back(syscall_json.asString());
      }
      syscalls_ = syscalls;

      std::stringstream ss;
      for (auto& s : syscalls_) ss << s << ",";
      CLOG(INFO) << "User configured syscalls=" << ss.str();
    }

    // Collection Method
    if (args->CollectionMethod().length() > 0) {
      collection_method_ = args->CollectionMethod();
      CLOG(INFO) << "User configured collection-method=" << collection_method_;
    } else if (!config["useEbpf"].empty()) {
      // useEbpf (deprecated)
      collection_method_ = config["useEbpf"].asBool() ? "ebpf" : "kernel_module";
      CLOG(INFO) << "User configured useEbpf=" << config["useEbpf"].asBool();
    }
  }

  if (const char* enable_sysdig_log = std::getenv("COLLECTOR_SD_LOG")) {
    std::string val(enable_sysdig_log);
    std::transform(val.begin(), val.end(), val.begin(), [](char c) {
      return std::tolower(c);
    });
    enable_sysdig_log_ = (val == "true");
  }

  if (disable_network_flows) {
    disable_network_flows_ = true;
  }

  if (ports_feature_flag) {
    scrape_listen_endpoints_ = true;
  }

  if (network_drop_ignored) {
    ignored_l4proto_port_pairs_ = kIgnoredL4ProtoPortPairs;
  }

  if (alternate_probe_download) {
    alternate_probe_download_ = true;
  }

  if (set_curl_verbose) {
    curl_verbose_ = true;
  }

  host_config_ = ProcessHostHeuristics(*this);
}

bool CollectorConfig::UseChiselCache() const {
  return use_chisel_cache_;
}

bool CollectorConfig::UseEbpf() const {
  if (host_config_.HasCollectionMethod()) {
    return host_config_.CollectionMethod() == "ebpf";
  }
  return (collection_method_ == "ebpf");
}

bool CollectorConfig::TurnOffScrape() const {
  return turn_off_scrape_;
}

int CollectorConfig::ScrapeInterval() const {
  return scrape_interval_;
}

int CollectorConfig::SnapLen() const {
  return snap_len_;
}

std::string CollectorConfig::Chisel() const {
  return chisel_;
}

std::string CollectorConfig::CollectionMethod() const {
  if (host_config_.HasCollectionMethod()) {
    return host_config_.CollectionMethod();
  }
  return collection_method_;
}

std::string CollectorConfig::Hostname() const {
  return hostname_;
}

std::string CollectorConfig::HostProc() const {
  return host_proc_;
}

std::vector<std::string> CollectorConfig::Syscalls() const {
  return syscalls_;
}

std::string CollectorConfig::LogLevel() const {
  return logging::GetLogLevelName(logging::GetLogLevel());
}

std::ostream& operator<<(std::ostream& os, const CollectorConfig& c) {
  return os
         << "collection_method:" << c.CollectionMethod()
         << ", useChiselCache:" << c.UseChiselCache()
         << ", snapLen:" << c.SnapLen()
         << ", scrape_interval:" << c.ScrapeInterval()
         << ", turn_off_scrape:" << c.TurnOffScrape()
         << ", hostname:" << c.Hostname()
         << ", logLevel:" << c.LogLevel();
}

}  // namespace collector
