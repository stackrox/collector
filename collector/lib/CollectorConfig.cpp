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

// If true, set curl to be verbose, adding further logging that might be useful for debugging.
BoolEnvVar set_curl_verbose("ROX_COLLECTOR_SET_CURL_VERBOSE", false);

BoolEnvVar set_enable_afterglow("ROX_ENABLE_AFTERGLOW", true);

BoolEnvVar set_enable_core_dump("ENABLE_CORE_DUMP", false);

// If true, add originator process information in NetworkEndpoint
BoolEnvVar set_processes_listening_on_ports("ROX_PROCESSES_LISTENING_ON_PORT", CollectorConfig::kEnableProcessesListeningOnPorts);

BoolEnvVar core_bpf_hardfail("ROX_COLLECTOR_CORE_BPF_HARDFAIL", false);

BoolEnvVar set_import_users("ROX_COLLECTOR_SET_IMPORT_USERS", false);

BoolEnvVar collect_connection_status("ROX_COLLECT_CONNECTION_STATUS", true);

BoolEnvVar aggregate_unmatched_ip("ROX_COLLECTOR_AGGREGATE_UNMATCHED_IP", true);

}  // namespace

constexpr bool CollectorConfig::kTurnOffScrape;
constexpr int CollectorConfig::kScrapeInterval;
constexpr CollectionMethod CollectorConfig::kCollectionMethod;
constexpr const char* CollectorConfig::kSyscalls[];
constexpr bool CollectorConfig::kEnableProcessesListeningOnPorts;

const UnorderedSet<L4ProtoPortPair> CollectorConfig::kIgnoredL4ProtoPortPairs = {{L4Proto::UDP, 9}};
;

CollectorConfig::CollectorConfig(CollectorArgs* args) {
  // Set default configuration values
  scrape_interval_ = kScrapeInterval;
  turn_off_scrape_ = kTurnOffScrape;
  collection_method_ = kCollectionMethod;
  enable_processes_listening_on_ports_ = set_processes_listening_on_ports.value();
  core_bpf_hardfail_ = core_bpf_hardfail.value();
  import_users_ = set_import_users.value();
  collect_connection_status_ = collect_connection_status.value();
  aggregate_unmatched_ip_ = aggregate_unmatched_ip.value();

  for (const auto& syscall : kSyscalls) {
    syscalls_.push_back(syscall);
  }

  // Get hostname
  hostname_ = GetHostname();
  if (hostname_.empty()) {
    CLOG(FATAL) << "Unable to determine the hostname. Consider setting the environment variable NODE_HOSTNAME";
  }

  // Get path to host proc dir
  host_proc_ = GetHostPath("/proc");

  // Check user provided configuration
  if (args) {
    auto config = args->CollectorConfig();

    // Log Level
    // process this first to ensure logging behaves correctly
    if (!config["logLevel"].empty()) {
      logging::LogLevel level;
      if (logging::ParseLogLevelName(config["logLevel"].asString(), &level)) {
        logging::SetLogLevel(level);
        CLOG(INFO) << "User configured logLevel=" << config["logLevel"].asString();
      } else {
        CLOG(INFO) << "User configured logLevel is invalid " << config["logLevel"].asString();
      }
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
    if (args->GetCollectionMethod().length() > 0) {
      const auto& cm = args->GetCollectionMethod();

      CLOG(INFO) << "User configured collection-method=" << cm;
      if (cm == "ebpf") {
        collection_method_ = CollectionMethod::EBPF;
      } else if (cm == "core_bpf") {
        collection_method_ = CollectionMethod::CORE_BPF;
      } else {
        CLOG(WARNING) << "Invalid collection-method (" << cm << "), using eBPF";
        collection_method_ = CollectionMethod::EBPF;
      }
    }

    if (!config["tlsConfig"].empty()) {
      tls_config_ = config["tlsConfig"];
    }
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

  if (set_curl_verbose) {
    curl_verbose_ = true;
  }

  if (set_enable_core_dump) {
    enable_core_dump_ = true;
  }

  HandleAfterglowEnvVars();

  host_config_ = ProcessHostHeuristics(*this);
}

void CollectorConfig::HandleAfterglowEnvVars() {
  if (!set_enable_afterglow) {
    enable_afterglow_ = false;
  }

  if (const char* afterglow_period = std::getenv("ROX_AFTERGLOW_PERIOD")) {
    afterglow_period_micros_ = static_cast<int64_t>(atof(afterglow_period) * 1000000);
  }

  if (enable_afterglow_ && afterglow_period_micros_ > 0) {
    CLOG(INFO) << "Afterglow is enabled";
    return;
  }

  if (!enable_afterglow_) {
    CLOG(INFO) << "Afterglow is disabled";
    return;
  }

  if (afterglow_period_micros_ < 0) {
    CLOG(WARNING) << "Invalid afterglow period " << afterglow_period_micros_ / 1000000 << ". ROX_AFTERGLOW_PERIOD must be positive.";
  } else {
    CLOG(WARNING) << "Afterglow period set to 0";
  }

  enable_afterglow_ = false;
  CLOG(INFO) << "Disabling afterglow";
}

bool CollectorConfig::TurnOffScrape() const {
  return turn_off_scrape_;
}

int CollectorConfig::ScrapeInterval() const {
  return scrape_interval_;
}

CollectionMethod CollectorConfig::GetCollectionMethod() const {
  if (host_config_.HasCollectionMethod()) {
    return host_config_.GetCollectionMethod();
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

int64_t CollectorConfig::AfterglowPeriod() const {
  return afterglow_period_micros_;
}

bool CollectorConfig::IsCoreDumpEnabled() const {
  return enable_core_dump_;
}

std::ostream& operator<<(std::ostream& os, const CollectorConfig& c) {
  return os
         << "collection_method:" << c.GetCollectionMethod()
         << ", scrape_interval:" << c.ScrapeInterval()
         << ", turn_off_scrape:" << c.TurnOffScrape()
         << ", hostname:" << c.Hostname()
         << ", processesListeningOnPorts:" << c.IsProcessesListeningOnPortsEnabled()
         << ", logLevel:" << c.LogLevel()
         << ", set_import_users:" << c.ImportUsers()
         << ", collect_connection_status:" << c.CollectConnectionStatus()
         << ", aggregate_unmatched_ip:" << c.AggregateUnmatchedIp();
}

}  // namespace collector
