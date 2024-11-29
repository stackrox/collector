#include "CollectorConfig.h"

#include <sstream>

#include <libsinsp/sinsp.h>

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

// Connection endpoints matching a network prefix listed here will be ignored.
// The default value contains link-local addresses for IPv4 (RFC3927) and IPv6 (RFC2462)
StringListEnvVar ignored_networks("ROX_IGNORE_NETWORKS", std::vector<std::string>({"169.254.0.0/16", "fe80::/10"}));

// Connection endpoints matching a network prefix listed here will never be aggregated.
StringListEnvVar non_aggregated_networks("ROX_NON_AGGREGATED_NETWORKS", std::vector<std::string>());

// If true, set curl to be verbose, adding further logging that might be useful for debugging.
BoolEnvVar set_curl_verbose("ROX_COLLECTOR_SET_CURL_VERBOSE", false);

BoolEnvVar set_enable_afterglow("ROX_ENABLE_AFTERGLOW", true);

BoolEnvVar set_enable_core_dump("ENABLE_CORE_DUMP", false);

// If true, add originator process information in NetworkEndpoint
BoolEnvVar set_processes_listening_on_ports("ROX_PROCESSES_LISTENING_ON_PORT", CollectorConfig::kEnableProcessesListeningOnPorts);

BoolEnvVar set_import_users("ROX_COLLECTOR_SET_IMPORT_USERS", false);

BoolEnvVar collect_connection_status("ROX_COLLECT_CONNECTION_STATUS", true);

BoolEnvVar enable_external_ips("ROX_ENABLE_EXTERNAL_IPS", false);

BoolEnvVar enable_connection_stats("ROX_COLLECTOR_ENABLE_CONNECTION_STATS", true);

BoolEnvVar enable_detailed_metrics("ROX_COLLECTOR_ENABLE_DETAILED_METRICS", true);

BoolEnvVar enable_runtime_config("ROX_COLLECTOR_RUNTIME_CONFIG_ENABLED", false);

BoolEnvVar use_docker_ce("ROX_COLLECTOR_CE_USE_DOCKER", false);
BoolEnvVar use_podman_ce("ROX_COLLECTOR_CE_USE_PODMAN", false);

BoolEnvVar enable_introspection("ROX_COLLECTOR_INTROSPECTION_ENABLE", false);

BoolEnvVar disable_process_arguments("ROX_COLLECTOR_NO_PROCESS_ARGUMENTS", false);

}  // namespace

constexpr bool CollectorConfig::kTurnOffScrape;
constexpr int CollectorConfig::kScrapeInterval;
constexpr CollectionMethod CollectorConfig::kCollectionMethod;
constexpr const char* CollectorConfig::kSyscalls[];
constexpr bool CollectorConfig::kEnableProcessesListeningOnPorts;

const UnorderedSet<L4ProtoPortPair> CollectorConfig::kIgnoredL4ProtoPortPairs = {{L4Proto::UDP, 9}};
;

CollectorConfig::CollectorConfig() {
  // Set default configuration values
  scrape_interval_ = kScrapeInterval;
  turn_off_scrape_ = kTurnOffScrape;
  collection_method_ = kCollectionMethod;
}

void CollectorConfig::InitCollectorConfig(CollectorArgs* args) {
  enable_processes_listening_on_ports_ = set_processes_listening_on_ports.value();
  import_users_ = set_import_users.value();
  collect_connection_status_ = collect_connection_status.value();
  enable_external_ips_ = enable_external_ips.value();
  enable_connection_stats_ = enable_connection_stats.value();
  enable_detailed_metrics_ = enable_detailed_metrics.value();
  enable_runtime_config_ = enable_runtime_config.value();
  use_docker_ce_ = use_docker_ce.value();
  use_podman_ce_ = use_podman_ce.value();
  enable_introspection_ = enable_introspection.value();
  disable_process_arguments_ = disable_process_arguments.value();

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
        CLOG(WARNING) << "Invalid collection-method (" << cm << "), using CO-RE BPF";
        collection_method_ = CollectionMethod::CORE_BPF;
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

  for (const std::string& str : ignored_networks.value()) {
    if (str.empty())
      continue;

    std::optional<IPNet> net = IPNet::parse(str);

    if (net) {
      CLOG(INFO) << "Ignore network : " << *net;
      ignored_networks_.emplace_back(std::move(*net));
    } else {
      CLOG(ERROR) << "Invalid network in ROX_IGNORE_NETWORKS : " << str;
    }
  }

  for (const std::string& str : non_aggregated_networks.value()) {
    if (str.empty())
      continue;

    std::optional<IPNet> net = IPNet::parse(str);

    if (net) {
      CLOG(INFO) << "Non-aggregated network : " << *net;
      non_aggregated_networks_.emplace_back(std::move(*net));
    } else {
      CLOG(ERROR) << "Invalid network in ROX_NON_AGGREGATED_NETWORKS : " << str;
    }
  }

  if (set_curl_verbose) {
    curl_verbose_ = true;
  }

  if (set_enable_core_dump) {
    enable_core_dump_ = true;
  }

  HandleAfterglowEnvVars();
  HandleConnectionStatsEnvVars();
  HandleSinspEnvVars();

  host_config_ = ProcessHostHeuristics(*this);
}

void CollectorConfig::HandleAfterglowEnvVars() {
  if (!set_enable_afterglow) {
    enable_afterglow_ = false;
  }

  if (const char* afterglow_period = std::getenv("ROX_AFTERGLOW_PERIOD")) {
    afterglow_period_micros_ = static_cast<int64_t>(atof(afterglow_period) * 1000000);
  }

  const int64_t max_afterglow_period_micros = 300000000;  // 5 minutes

  if (afterglow_period_micros_ > max_afterglow_period_micros) {
    CLOG(ERROR) << "User set afterglow period of " << afterglow_period_micros_ / 1000000
                << "s is greater than the maximum allowed afterglow period of " << max_afterglow_period_micros / 1000000 << "s";
    CLOG(ERROR) << "Setting the afterglow period to " << max_afterglow_period_micros / 1000000 << "s";
    afterglow_period_micros_ = max_afterglow_period_micros;
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
    CLOG(ERROR) << "Invalid afterglow period " << afterglow_period_micros_ / 1000000 << ". ROX_AFTERGLOW_PERIOD must be positive.";
  } else {
    CLOG(ERROR) << "Afterglow period set to 0";
  }

  enable_afterglow_ = false;
  CLOG(INFO) << "Disabling afterglow";
}

void CollectorConfig::HandleConnectionStatsEnvVars() {
  const char* envvar;

  connection_stats_quantiles_ = {0.50, 0.90, 0.95};

  if ((envvar = std::getenv("ROX_COLLECTOR_CONNECTION_STATS_QUANTILES")) != NULL) {
    connection_stats_quantiles_.clear();
    std::stringstream quantiles(envvar);
    while (quantiles.good()) {
      std::string quantile;
      std::getline(quantiles, quantile, ',');
      try {
        double v = std::stod(quantile);
        connection_stats_quantiles_.push_back(v);
        CLOG(INFO) << "Connection statistics quantile: " << v;
      } catch (...) {
        CLOG(ERROR) << "Invalid quantile value: '" << quantile << "'";
      }
    }
  }

  connection_stats_error_ = 0.01;

  if ((envvar = std::getenv("ROX_COLLECTOR_CONNECTION_STATS_ERROR")) != NULL) {
    try {
      connection_stats_error_ = std::stod(envvar);
      CLOG(INFO) << "Connection statistics error value: " << connection_stats_error_;
    } catch (...) {
      CLOG(ERROR) << "Invalid quantile error value: '" << envvar << "'";
    }
  }

  connection_stats_window_ = 60;

  if ((envvar = std::getenv("ROX_COLLECTOR_CONNECTION_STATS_WINDOW")) != NULL) {
    try {
      connection_stats_window_ = std::stoi(envvar);
      CLOG(INFO) << "Connection statistics window: " << connection_stats_window_;
    } catch (...) {
      CLOG(ERROR) << "Invalid window length value: '" << envvar << "'";
    }
  }
}

void CollectorConfig::HandleSinspEnvVars() {
  const char* envvar;

  sinsp_cpu_per_buffer_ = DEFAULT_CPU_FOR_EACH_BUFFER;
  sinsp_buffer_size_ = DEFAULT_DRIVER_BUFFER_BYTES_DIM;
  // the default values for sinsp_thread_cache_size_, sinsp_total_buffer_size_
  // are not picked up from Falco, but set in the CollectorConfig class.

  if ((envvar = std::getenv("ROX_COLLECTOR_SINSP_CPU_PER_BUFFER")) != NULL) {
    try {
      sinsp_cpu_per_buffer_ = std::stoi(envvar);
      CLOG(INFO) << "Sinsp cpu per buffer: " << sinsp_cpu_per_buffer_;
    } catch (...) {
      CLOG(ERROR) << "Invalid cpu per buffer value: '" << envvar << "'";
    }
  }

  if ((envvar = std::getenv("ROX_COLLECTOR_SINSP_BUFFER_SIZE")) != NULL) {
    try {
      sinsp_buffer_size_ = std::stoll(envvar);
      CLOG(INFO) << "Sinsp buffer size: " << sinsp_buffer_size_;
    } catch (...) {
      CLOG(ERROR) << "Invalid buffer size value: '" << envvar << "'";
    }
  }

  if ((envvar = std::getenv("ROX_COLLECTOR_SINSP_TOTAL_BUFFER_SIZE")) != NULL) {
    try {
      sinsp_total_buffer_size_ = std::stoll(envvar);
      CLOG(INFO) << "Sinsp total buffer size: " << sinsp_total_buffer_size_;
    } catch (...) {
      CLOG(ERROR) << "Invalid total buffer size value: '" << envvar << "'";
    }
  }

  if ((envvar = std::getenv("ROX_COLLECTOR_SINSP_THREAD_CACHE_SIZE")) != NULL) {
    try {
      sinsp_thread_cache_size_ = std::stoll(envvar);
      CLOG(INFO) << "Sinsp thread cache size: " << sinsp_thread_cache_size_;
    } catch (...) {
      CLOG(ERROR) << "Invalid thread cache size value: '" << envvar << "'";
    }
  }
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
         << ", enable_detailed_metrics:" << c.EnableDetailedMetrics()
         << ", enable_external_ips:" << c.EnableExternalIPs();
}

// Returns size of ring buffers to be allocated.
// The value is adjusted based on the allowed total limit, e.g.:
//
// * total limit for ring buffers is 1Gi
// * one buffer takes 8Mi
// * there is one buffer per cpu
// * total number of CPU cores is 150
//
// In this scenario, buffers_total = (150 / cpu_per_buffer) * 8Mi > 1Gi. Thus
// the adjusted value will be returned: ceil(1Gi / 150) = 6Mi. Allocating one
// buffer of size 6Mi per CPU core will allow us to fit into the total limit.
//
unsigned int CollectorConfig::GetSinspBufferSize() const {
  unsigned int max_buffer_size, effective_buffer_size, n_buffers;

  if (sinsp_total_buffer_size_ == 0) {
    CLOG(WARNING) << "Trying to calculate buffer size without "
                     "requested total buffer size. Return unmodified "
                  << sinsp_buffer_size_ << " bytes.";
    return sinsp_buffer_size_;
  }

  if (sinsp_cpu_per_buffer_ == 0) {
    CLOG(WARNING) << "Trying to calculate buffer size without "
                     "requested cpu-per-buffer. Return unmodified "
                  << sinsp_buffer_size_ << " bytes.";
    return sinsp_buffer_size_;
  }

  if (host_config_.GetNumPossibleCPUs() == 0) {
    CLOG(WARNING) << "Trying to calculate buffer size without"
                     "number of possible CPUs. Return unmodified "
                  << sinsp_buffer_size_ << " bytes.";
    return sinsp_buffer_size_;
  }

  // Round to the larger value, since one buffer will be allocated even if the
  // last group of CPUs is less than sinsp_cpu_per_buffer_
  n_buffers = std::ceil((float)host_config_.GetNumPossibleCPUs() / (float)sinsp_cpu_per_buffer_);

  max_buffer_size = std::ceil((float)sinsp_total_buffer_size_ / (float)n_buffers);

  // Determine largest power of two that is not greater than max_buffer_size,
  // is a multiple of the page size (4096), and greater than two pages to meet
  // the requirements for ringbuffer dimensions in
  // falcosecurity-libs/driver/ppm_ringbuffer.h
  unsigned int maximum_power_of_two_exponent = static_cast<unsigned int>(std::log2(max_buffer_size));

  // The max_buffer_size could be arbitrary small here, so verify that the
  // resulting exponent value is large enough.
  //
  // A power of two is always a multiple of one page size if the exponent is
  // larger than 12 (2^12 = 4096). Falco also requires this value to be greater
  // than two pages, meaning that the exponent has to be at least 14.
  maximum_power_of_two_exponent = std::max((unsigned int)14, maximum_power_of_two_exponent);

  effective_buffer_size = std::min(sinsp_buffer_size_, (unsigned int)(1 << maximum_power_of_two_exponent));

  if (effective_buffer_size != sinsp_buffer_size_) {
    CLOG(INFO) << "Use modified ringbuf size of "
               << effective_buffer_size << " bytes.";
  }

  return effective_buffer_size;
}

void CollectorConfig::SetSinspBufferSize(unsigned int buffer_size) {
  sinsp_buffer_size_ = buffer_size;
}

void CollectorConfig::SetSinspTotalBufferSize(unsigned int total_buffer_size) {
  sinsp_total_buffer_size_ = total_buffer_size;
}

void CollectorConfig::SetHostConfig(HostConfig* config) {
  host_config_ = *config;
}

void CollectorConfig::SetSinspCpuPerBuffer(unsigned int cpu_per_buffer) {
  sinsp_cpu_per_buffer_ = cpu_per_buffer;
}

}  // namespace collector
