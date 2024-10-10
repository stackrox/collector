#include "CollectorConfig.h"

#include <filesystem>
#include <optional>

#include <libsinsp/sinsp.h>

#include "CollectionMethod.h"
#include "CollectorArgs.h"
#include "EnvVar.h"
#include "GRPC.h"
#include "HostHeuristics.h"
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
BoolEnvVar set_processes_listening_on_ports("ROX_PROCESSES_LISTENING_ON_PORT", true);

BoolEnvVar set_import_users("ROX_COLLECTOR_SET_IMPORT_USERS", false);

BoolEnvVar collect_connection_status("ROX_COLLECT_CONNECTION_STATUS", true);

BoolEnvVar enable_external_ips("ROX_ENABLE_EXTERNAL_IPS", false);

// Stats and metrics configuration
BoolEnvVar enable_connection_stats("ROX_COLLECTOR_ENABLE_CONNECTION_STATS", true);
BoolEnvVar enable_detailed_metrics("ROX_COLLECTOR_ENABLE_DETAILED_METRICS", true);
StringListEnvVar connection_stats_quantiles("ROX_COLLECTOR_CONNECTION_STATS_QUANTILES");
DoubleEnvVar connection_stats_error("ROX_COLLECTOR_CONNECTION_STATS_ERROR", 0.01);
UIntEnvVar connection_stats_window("ROX_COLLECTOR_CONNECTION_STATS_WINDOW", 60);

// Sinsp configuration
UIntEnvVar sinsp_cpu_per_buffer("ROX_COLLECTOR_SINSP_CPU_PER_BUFFER", DEFAULT_CPU_FOR_EACH_BUFFER);
UIntEnvVar sinsp_buffer_size("ROX_COLLECTOR_SINSP_BUFFER_SIZE", DEFAULT_DRIVER_BUFFER_BYTES_DIM);
UIntEnvVar sinsp_total_buffer_size("ROX_COLLECTOR_SINSP_TOTAL_BUFFER_SIZE", 512 * 1024 * 1024);
UIntEnvVar sinsp_thread_cache_size("ROX_COLLECTOR_SINSP_TOTAL_BUFFER_SIZE", 32768);

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

const UnorderedSet<L4ProtoPortPair> CollectorConfig::kIgnoredL4ProtoPortPairs = {{L4Proto::UDP, 9}};

CollectorConfig::CollectorConfig(CollectorArgs* args, bool skipHeuristics)
    : host_proc_(GetHostPath("/proc")),
      disable_network_flows_(disable_network_flows),
      scrape_listen_endpoints_(ports_feature_flag),
      enable_core_dump_(set_enable_core_dump),
      enable_processes_listening_on_ports_(set_processes_listening_on_ports),
      import_users_(set_import_users),
      collect_connection_status_(collect_connection_status),
      enable_external_ips_(enable_external_ips),
      enable_connection_stats_(enable_connection_stats),
      enable_detailed_metrics_(enable_detailed_metrics),
      enable_runtime_config_(enable_runtime_config),
      use_docker_ce_(use_docker_ce),
      use_podman_ce_(use_podman_ce),
      enable_introspection_(enable_introspection),
      track_send_recv_(track_send_recv),
      connection_stats_error_(connection_stats_error),
      connection_stats_window_(connection_stats_window),
      sinsp_cpu_per_buffer_(sinsp_cpu_per_buffer),
      sinsp_buffer_size_(sinsp_buffer_size),
      sinsp_total_buffer_size_(sinsp_total_buffer_size),
      sinsp_thread_cache_size_(sinsp_thread_cache_size) {
  // Get hostname
  hostname_ = GetHostname();
  if (hostname_.empty()) {
    CLOG(FATAL) << "Unable to determine the hostname. Consider setting the environment variable NODE_HOSTNAME";
  }

  for (const auto& syscall : kSyscalls) {
    syscalls_.emplace_back(syscall);
  }

  if (track_send_recv_) {
    for (const auto& syscall : kSendRecvSyscalls) {
      syscalls_.emplace_back(syscall);
    }
  }

  HandleArgs(args);
  HandleNetworkConfig();
  HandleAfterglowEnvVars();
  HandleConnectionStatsQuantiles();
  HandleConfig(config_file.value());

  if (!skipHeuristics) {
    host_config_ = ProcessHostHeuristics(*this);
  }
}

void CollectorConfig::HandleArgs(const CollectorArgs* args) {
  if (args == nullptr) {
    return;
  }
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

  HandleLogLevel(config, args);
  HandleScrapeConfig(config, args);
  HandleCollectionMethod(args);
  HandleTls(config, args);
}

void CollectorConfig::HandleLogLevel(const Json::Value& config, const CollectorArgs* args) {
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
}

void CollectorConfig::HandleScrapeConfig(const Json::Value& config, const CollectorArgs* args) {
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
}

void CollectorConfig::HandleCollectionMethod(const CollectorArgs* args) {
  auto setCollectionMethod = [&](const CollectionMethod cm) {
    CLOG(INFO) << "User configured collection-method=" << CollectionMethodName(cm);
    collection_method_ = cm;
  };

  if (args->GetCollectionMethod()) {
    setCollectionMethod(args->GetCollectionMethod().value());
  } else if (collection_method.hasValue()) {
    setCollectionMethod(ParseCollectionMethod(collection_method.value()));
  }
}

void CollectorConfig::HandleTls(const Json::Value& config, const CollectorArgs* args) {
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

void CollectorConfig::HandleNetworkConfig() {
  auto filler = [](std::vector<IPNet>& output, const std::vector<std::string>& input, const std::string& error_message, const std::string& notification_message) {
    for (const std::string& str : input) {
      if (str.empty()) {
        continue;
      }

      std::optional<IPNet> net = IPNet::parse(str);

      if (net) {
        CLOG(INFO) << notification_message << " : " << *net;
        output.emplace_back(std::move(*net));
      } else {
        CLOG(ERROR) << error_message << " : " << str;
      }
    }
  };

  if (network_drop_ignored) {
    ignored_l4proto_port_pairs_ = kIgnoredL4ProtoPortPairs;
  }

  filler(ignored_networks_, ignored_networks.value(), "Ignore network", "Invalid network in ROX_IGNORE_NETWORKS");
  filler(non_aggregated_networks_, non_aggregated_networks.value(), "Non-aggregated network", "Invalid network in ROX_NON_AGGREGATED_NETWORKS");
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

void CollectorConfig::HandleConnectionStatsQuantiles() {
  connection_stats_quantiles_ = {0.50, 0.90, 0.95};

  if (connection_stats_quantiles.hasValue()) {
    connection_stats_quantiles_.clear();
    for (const auto& quantile : connection_stats_quantiles.value()) {
      try {
        double v = std::stod(quantile);
        connection_stats_quantiles_.push_back(v);
        CLOG(INFO) << "Connection statistics quantile: " << v;
      } catch (...) {
        CLOG(ERROR) << "Invalid quantile value: '" << quantile << "'";
      }
    }
  }
}

void CollectorConfig::YamlConfigToConfig(YAML::Node& yamlConfig) {
  if (yamlConfig.IsNull() || !yamlConfig.IsDefined()) {
    CLOG(FATAL) << "Unable to read config from config file";
    return;
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
  CLOG(INFO) << "Runtime configuration:";
  CLOG(INFO) << GetRuntimeConfigStr();

  return;
}

void CollectorConfig::HandleConfig(const std::filesystem::path& filePath) {
  if (!std::filesystem::exists(filePath)) {
    CLOG(DEBUG) << "No configuration file found. " << filePath;
    return;
  }

  try {
    YAML::Node yamlConfig = YAML::LoadFile(filePath);
    YamlConfigToConfig(yamlConfig);
  } catch (const YAML::BadFile& e) {
    CLOG(FATAL) << "Failed to open the configuration file: " << filePath
                << ". Error: " << e.what();
  } catch (const YAML::ParserException& e) {
    CLOG(FATAL) << "Failed to parse the configuration file: " << filePath
                << ". Error: " << e.what();
  } catch (const YAML::Exception& e) {
    CLOG(FATAL) << "An error occurred while loading the configuration file: " << filePath
                << ". Error: " << e.what();
  } catch (const std::exception& e) {
    CLOG(FATAL) << "An unexpected error occurred while trying to read: " << filePath << e.what();
  }
}

CollectionMethod CollectorConfig::GetCollectionMethod() const {
  if (host_config_.HasCollectionMethod()) {
    return host_config_.GetCollectionMethod();
  }
  return collection_method_;
}

std::string CollectorConfig::LogLevel() {
  return logging::GetLogLevelName(logging::GetLogLevel());
}

std::ostream& operator<<(std::ostream& os, const CollectorConfig& c) {
  return os
         << "collection_method:" << c.GetCollectionMethod()
         << ", scrape_interval:" << c.ScrapeInterval()
         << ", turn_off_scrape:" << c.TurnOffScrape()
         << ", hostname:" << c.Hostname()
         << ", processesListeningOnPorts:" << c.IsProcessesListeningOnPortsEnabled()
         << ", logLevel:" << CollectorConfig::LogLevel()
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
