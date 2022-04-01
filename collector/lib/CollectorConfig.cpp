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

}  // namespace

constexpr bool CollectorConfig::kUseChiselCache;
constexpr bool CollectorConfig::kSnapLen;
constexpr bool CollectorConfig::kTurnOffScrape;
constexpr int CollectorConfig::kScrapeInterval;
constexpr char CollectorConfig::kCollectionMethod[];
constexpr char CollectorConfig::kChisel[];
constexpr const char* CollectorConfig::kSyscalls[];
constexpr bool CollectorConfig::kForceKernelModules;

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
  force_kernel_modules_ = kForceKernelModules;

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

    // Force kernel modules collection method
    if (!config["forceKernelModules"].empty()) {
      force_kernel_modules_ = config["forceKernelModules"].asBool();
      CLOG(INFO) << "User configured forceKernelModules=" << force_kernel_modules_;
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

int64_t CollectorConfig::AfterglowPeriod() const {
  return afterglow_period_micros_;
}

bool CollectorConfig::IsCoreDumpEnabled() const {
  return enable_core_dump_;
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
