#include "CollectorConfig.h"

#include <filesystem>
#include <optional>
#include <sstream>

#include <sys/inotify.h>

#include <libsinsp/sinsp.h>

#include "CollectionMethod.h"
#include "CollectorArgs.h"
#include "EnvVar.h"
#include "GRPC.h"
#include "HostHeuristics.h"
#include "HostInfo.h"
#include "Logging.h"
#include "TlsConfig.h"
#include "Utility.h"
#include "optionparser.h"

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

BoolEnvVar enable_afterglow("ROX_ENABLE_AFTERGLOW", true);
FloatEnvVar afterglow_period("ROX_AFTERGLOW_PERIOD", 300.0);

BoolEnvVar set_enable_core_dump("ENABLE_CORE_DUMP", false);

// If true, add originator process information in NetworkEndpoint
BoolEnvVar set_processes_listening_on_ports("ROX_PROCESSES_LISTENING_ON_PORT", CollectorConfig::kEnableProcessesListeningOnPorts);

BoolEnvVar set_import_users("ROX_COLLECTOR_SET_IMPORT_USERS", false);

BoolEnvVar collect_connection_status("ROX_COLLECT_CONNECTION_STATUS", true);

BoolEnvVar enable_external_ips("ROX_COLLECTOR_EXTERNAL_IPS_ENABLE", false);

BoolEnvVar enable_connection_stats("ROX_COLLECTOR_ENABLE_CONNECTION_STATS", true);

BoolEnvVar enable_detailed_metrics("ROX_COLLECTOR_ENABLE_DETAILED_METRICS", true);

BoolEnvVar enable_runtime_config("ROX_COLLECTOR_RUNTIME_CONFIG_ENABLED", false);

BoolEnvVar use_docker_ce("ROX_COLLECTOR_CE_USE_DOCKER", false);
BoolEnvVar use_podman_ce("ROX_COLLECTOR_CE_USE_PODMAN", false);

BoolEnvVar enable_introspection("ROX_COLLECTOR_INTROSPECTION_ENABLE", false);

BoolEnvVar track_send_recv("ROX_COLLECTOR_TRACK_SEND_RECV", false);

// Collector arguments alternatives
StringEnvVar log_level("ROX_COLLECTOR_LOG_LEVEL");
IntEnvVar scrape_interval("ROX_COLLECTOR_SCRAPE_INTERVAL");
BoolEnvVar scrape_off("ROX_COLLECTOR_SCRAPE_DISABLED");
StringEnvVar grpc_server("GRPC_SERVER");
StringEnvVar collector_config("COLLECTOR_CONFIG");
StringEnvVar collection_method("COLLECTION_METHOD");

// TLS Configuration
PathEnvVar tls_certs_path("ROX_COLLECTOR_TLS_CERTS");
PathEnvVar tls_ca_path("ROX_COLLECTOR_TLS_CA");
PathEnvVar tls_client_cert_path("ROX_COLLECTOR_TLS_CLIENT_CERT");
PathEnvVar tls_client_key_path("ROX_COLLECTOR_TLS_CLIENT_KEY");

PathEnvVar config_file("ROX_COLLECTOR_CONFIG_PATH", "/etc/stackrox/runtime_config.yaml");

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
  track_send_recv_ = track_send_recv.value();

  for (const auto& syscall : kSyscalls) {
    syscalls_.emplace_back(syscall);
  }

  if (track_send_recv_) {
    for (const auto& syscall : kSendRecvSyscalls) {
      syscalls_.emplace_back(syscall);
    }
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

    if (config.empty() && collector_config.hasValue()) {
      Json::Value root;
      const auto& [status, msg] = CheckConfiguration(collector_config.value().c_str(), &root);
      if (!msg.empty()) {
        CLOG(INFO) << msg;
      }

      if (status == option::ARG_OK) {
        config = root;
      }
    }

    // Log Level
    // process this first to ensure logging behaves correctly
    auto setLogLevel = [](const std::string& level) {
      logging::LogLevel lvl;
      if (logging::ParseLogLevelName(level, &lvl)) {
        logging::SetLogLevel(lvl);
        CLOG(INFO) << "User configured logLevel=" << level;
      } else {
        CLOG(INFO) << "User configured logLevel is invalid " << level;
      }
    };
    if (!config["logLevel"].empty()) {
      setLogLevel(config["logLevel"].asString());
    } else if (log_level.hasValue()) {
      setLogLevel(log_level.value());
    }

    // Scrape Interval
    auto setScrapeInterval = [&](int interval) {
      scrape_interval_ = interval;
      CLOG(INFO) << "User configured scrapeInterval=" << scrape_interval_;
    };
    if (!config["scrapeInterval"].empty()) {
      setScrapeInterval(config["scrapeInterval"].asInt());
    } else if (scrape_interval.hasValue()) {
      setScrapeInterval(scrape_interval.value());
    }

    // Scrape Enabled/Disabled
    auto setScrapeOff = [&](bool off) {
      turn_off_scrape_ = off;
      CLOG(INFO) << "User configured turnOffScrape=" << turn_off_scrape_;
    };

    if (!config["turnOffScrape"].empty()) {
      setScrapeOff(config["turnOffScrape"].asBool());
    } else if (scrape_off.hasValue()) {
      setScrapeOff(scrape_off.value());
    }

    // Collection Method
    auto setCollectionMethod = [&](const CollectionMethod cm) {
      CLOG(INFO) << "User configured collection-method=" << CollectionMethodName(cm);
      collection_method_ = cm;
    };

    if (args->GetCollectionMethod()) {
      setCollectionMethod(args->GetCollectionMethod().value());
    } else if (collection_method.hasValue()) {
      setCollectionMethod(ParseCollectionMethod(collection_method.value()));
    }

    // TLS configuration
    std::filesystem::path ca_cert_path;
    std::filesystem::path client_cert_path;
    std::filesystem::path client_key_path;
    if (!config["tlsConfig"].empty()) {
      ca_cert_path = config["tlsConfig"]["caCertPath"].asString();
      client_cert_path = config["tlsConfig"]["clientCertPath"].asString();
      client_key_path = config["tlsConfig"]["clientKeyPath"].asString();
    } else if (tls_certs_path.hasValue()) {
      const std::filesystem::path& tls_base_path = tls_certs_path.value();
      ca_cert_path = tls_ca_path.valueOr(tls_base_path / "ca.pem");
      client_cert_path = tls_client_cert_path.valueOr(tls_base_path / "cert.pem");
      client_key_path = tls_client_key_path.valueOr(tls_base_path / "key.pem");
    } else {
      ca_cert_path = tls_ca_path.valueOr("");
      client_cert_path = tls_client_cert_path.valueOr("");
      client_key_path = tls_client_key_path.valueOr("");
    }
    tls_config_ = TlsConfig(ca_cert_path, client_cert_path, client_key_path);

    if (!args->GRPCServer().empty()) {
      grpc_server_ = args->GRPCServer();
    } else if (grpc_server.hasValue()) {
      const auto& [status, msg] = CheckGrpcServer(grpc_server.value());
      if (status == option::ARG_OK) {
        grpc_server_ = grpc_server.value();
      } else if (!msg.empty()) {
        CLOG(INFO) << msg;
      }
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
    if (str.empty()) {
      continue;
    }

    std::optional<IPNet> net = IPNet::parse(str);

    if (net) {
      CLOG(INFO) << "Ignore network : " << *net;
      ignored_networks_.emplace_back(std::move(*net));
    } else {
      CLOG(ERROR) << "Invalid network in ROX_IGNORE_NETWORKS : " << str;
    }
  }

  for (const std::string& str : non_aggregated_networks.value()) {
    if (str.empty()) {
      continue;
    }

    std::optional<IPNet> net = IPNet::parse(str);

    if (net) {
      CLOG(INFO) << "Non-aggregated network : " << *net;
      non_aggregated_networks_.emplace_back(std::move(*net));
    } else {
      CLOG(ERROR) << "Invalid network in ROX_NON_AGGREGATED_NETWORKS : " << str;
    }
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
  constexpr int64_t SECOND = 1'000'000;
  constexpr int64_t max_afterglow_period_micros = 300 * SECOND;  // 5 minutes

  afterglow_period_micros_ = static_cast<uint64_t>(afterglow_period.value() * SECOND);

  if (afterglow_period_micros_ < 0) {
    CLOG(ERROR) << "Invalid afterglow period " << afterglow_period_micros_ / SECOND << ". ROX_AFTERGLOW_PERIOD must be positive.";
  } else if (afterglow_period_micros_ == 0) {
    CLOG(ERROR) << "Afterglow period set to 0.";
  } else {
    if (afterglow_period_micros_ > max_afterglow_period_micros) {
      CLOG(WARNING) << "User set afterglow period of " << afterglow_period_micros_ / SECOND
                    << "s is greater than the maximum allowed afterglow period of " << max_afterglow_period_micros / SECOND << "s";
      CLOG(WARNING) << "Setting the afterglow period to " << max_afterglow_period_micros / SECOND << "s";
      afterglow_period_micros_ = max_afterglow_period_micros;
    }

    enable_afterglow_ = enable_afterglow.value();
  }

  CLOG(INFO) << "Afterglow is " << (enable_afterglow_ ? "enabled" : "disabled");
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

void CollectorConfig::YamlConfigToConfig(YAML::Node& yamlConfig) {
  // Don't read the file during a scrape
  std::unique_lock<std::shared_mutex> lock(mutex_);

  if (yamlConfig.IsNull() || !yamlConfig.IsDefined()) {
    throw std::runtime_error("Unable to read config from config file");
  }
  YAML::Node networking = yamlConfig["networking"];
  if (!networking) {
    CLOG(INFO) << "No networking in config file";
    return;
  }

  bool enableExternalIps = false;
  YAML::Node externalIpsNode = networking["externalIps"];
  if (!externalIpsNode) {
    CLOG(INFO) << "No external IPs in config file";
    return;
  }
  enableExternalIps = externalIpsNode["enable"].as<bool>(false);

  sensor::CollectorConfig runtime_config;
  auto* networkingConfig = runtime_config.mutable_networking();
  auto* externalIpsConfig = networkingConfig->mutable_external_ips();
  externalIpsConfig->set_enable(enableExternalIps);

  SetRuntimeConfig(runtime_config);
  CLOG(INFO) << "Runtime configuration:\n"
             << GetRuntimeConfigStr();

  return;
}

bool CollectorConfig::HandleConfig(const std::filesystem::path& filePath) {
  if (!std::filesystem::exists(filePath)) {
    CLOG(DEBUG) << "No configuration file found. " << filePath;
    return true;
  }

  try {
    YAML::Node yamlConfig = YAML::LoadFile(filePath);
    YamlConfigToConfig(yamlConfig);
  } catch (const YAML::BadFile& e) {
    CLOG(ERROR) << "Failed to open the configuration file: " << filePath << ". Error: " << e.what();
    return false;
  } catch (const YAML::ParserException& e) {
    CLOG(ERROR) << "Failed to parse the configuration file: " << filePath << ". Error: " << e.what();
    return false;
  } catch (const YAML::Exception& e) {
    CLOG(ERROR) << "An error occurred while loading the configuration file: " << filePath << ". Error: " << e.what();
    return false;
  } catch (const std::exception& e) {
    CLOG(ERROR) << "An unexpected error occurred while trying to read: " << filePath << e.what();
    return false;
  }

  return true;
}

void CollectorConfig::WaitForFileToExist(const std::filesystem::path& filePath) {
  int count = 0;
  while (!std::filesystem::exists(filePath) && !thread_.should_stop()) {
    sleep(1);
    count++;
    if (count > 45) {
      std::unique_lock<std::shared_mutex> lock(mutex_);
      runtime_config_.reset();
    }
  }
}

int CollectorConfig::WaitForInotifyAddWatch(int fd, const std::filesystem::path& filePath) {
  while (!thread_.should_stop()) {
    int wd = inotify_add_watch(fd, filePath.c_str(), IN_MODIFY | IN_MOVE_SELF | IN_DELETE_SELF);
    if (wd < 0) {
      CLOG_THROTTLED(ERROR, std::chrono::seconds(30)) << "Failed to add inotify watch for " << filePath;
    } else {
      return wd;
    }
    sleep(1);
  }

  return -1;
}

void CollectorConfig::WatchConfigFile(const std::filesystem::path& filePath) {
  int fd = inotify_init();
  if (fd < 0) {
    CLOG(ERROR) << "inotify_init() failed: " << StrError();
    CLOG(ERROR) << "Runtime configuration will not be used.";
    return;
  }
  WaitForFileToExist(filePath);
  int wd = WaitForInotifyAddWatch(fd, filePath);
  bool success = HandleConfig(filePath);
  if (!success) {
    CLOG(FATAL) << "Unable to parse configuration file: " << filePath;
  }

  char buffer[1024];
  while (!thread_.should_stop()) {
    int length = read(fd, buffer, sizeof(buffer));
    if (length < 0) {
      CLOG_THROTTLED(ERROR, std::chrono::seconds(30)) << "Unable to read event for " << filePath;
    }

    struct inotify_event* event;
    for (int i = 0; i < length; i += sizeof(struct inotify_event) + event->len) {
      event = (struct inotify_event*)&buffer[i];
      if (event->mask & IN_MODIFY) {
        HandleConfig(filePath);
      } else if ((event->mask & IN_MOVE_SELF) || (event->mask & IN_DELETE_SELF) || (event->mask & IN_IGNORED)) {
        WaitForFileToExist(filePath);
        inotify_rm_watch(fd, wd);
        fd = inotify_init();
        if (fd < 0) {
          CLOG(ERROR) << "inotify_init() failed: " << StrError();
          CLOG(ERROR) << "Runtime configuration will no longer be used. Reverting to default values.";
          std::unique_lock<std::shared_mutex> lock(mutex_);
          runtime_config_.reset();
          return;
        }
        wd = WaitForInotifyAddWatch(fd, filePath);
        HandleConfig(filePath);
      }
    }
  }

  CLOG(INFO) << "No longer using inotify on " << filePath;
  inotify_rm_watch(fd, wd);
  close(fd);
}

void CollectorConfig::Start() {
  thread_.Start([this] { WatchConfigFile(config_file.value()); });
  CLOG(INFO) << "Watching config file: " << config_file.value();
}

void CollectorConfig::Stop() {
  thread_.Stop();
  CLOG(INFO) << "No longer watching config file" << config_file.value();
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
         << ", enable_external_ips:" << c.EnableExternalIPs()
         << ", track_send_recv:" << c.TrackingSendRecv();
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

std::pair<option::ArgStatus, std::string> CollectorConfig::CheckConfiguration(const char* config, Json::Value* root) {
  using namespace option;
  assert(root != nullptr);

  if (config == nullptr) {
    return std::make_pair(ARG_IGNORE, "Missing collector config");
  }

  CLOG(DEBUG) << "Incoming: " << config;

  Json::Reader reader;
  if (!reader.parse(config, *root)) {
    std::string msg = "A valid JSON configuration is required to start the collector: ";
    msg += reader.getFormattedErrorMessages();
    return std::make_pair(ARG_ILLEGAL, msg);
  }

  return std::make_pair(ARG_OK, "");
}

}  // namespace collector
