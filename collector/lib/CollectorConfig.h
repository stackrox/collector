#ifndef _COLLECTOR_CONFIG_H_
#define _COLLECTOR_CONFIG_H_

#include <optional>
#include <ostream>
#include <vector>

#include <gtest/gtest_prod.h>
#include <json/json.h>
#include <yaml-cpp/yaml.h>

#include <grpcpp/channel.h>

#include <internalapi/sensor/collector.pb.h>

#include "CollectionMethod.h"
#include "HostConfig.h"
#include "NetworkConnection.h"
#include "TlsConfig.h"
#include "json/value.h"
#include "optionparser.h"

namespace collector {

class CollectorArgs;

class CollectorConfig {
 public:
  static constexpr bool kTurnOffScrape = false;
  static constexpr int kScrapeInterval = 30;
  static constexpr CollectionMethod kCollectionMethod = CollectionMethod::CORE_BPF;
  static constexpr const char* kSyscalls[] = {
      "accept",
      "accept4",
      "chdir",
      "clone",
      "close",
      "connect",
      "execve",
      "fchdir",
      "fork",
      "getsockopt",
      "procexit",
      "procinfo",
      "setresgid",
      "setresuid",
      "setgid",
      "setuid",
      "shutdown",
      "socket",
#ifdef __s390x__
      "syscall",
#endif
      "vfork",
  };
  static constexpr const char* kSendRecvSyscalls[] = {
      "sendto",
      "sendmsg",
      "sendmmsg",
      "recvfrom",
      "recvmsg",
      "recvmmsg",
  };
  static const UnorderedSet<L4ProtoPortPair> kIgnoredL4ProtoPortPairs;
  static constexpr bool kEnableProcessesListeningOnPorts = true;

  CollectorConfig();
  void InitCollectorConfig(CollectorArgs* collectorArgs);

  std::string asString() const;

  bool TurnOffScrape() const;
  bool ScrapeListenEndpoints() const { return scrape_listen_endpoints_; }
  int ScrapeInterval() const;
  std::string Hostname() const;
  std::string HostProc() const;
  CollectionMethod GetCollectionMethod() const;
  std::vector<std::string> Syscalls() const;
  int64_t AfterglowPeriod() const;
  std::string LogLevel() const;
  bool DisableNetworkFlows() const { return disable_network_flows_; }
  const UnorderedSet<L4ProtoPortPair>& IgnoredL4ProtoPortPairs() const { return ignored_l4proto_port_pairs_; }
  const std::vector<IPNet>& IgnoredNetworks() const { return ignored_networks_; }
  const std::vector<IPNet>& NonAggregatedNetworks() const { return non_aggregated_networks_; }
  bool EnableAfterglow() const { return enable_afterglow_; }
  bool IsCoreDumpEnabled() const;
  const std::optional<TlsConfig>& TLSConfiguration() const { return tls_config_; }
  bool IsProcessesListeningOnPortsEnabled() const { return enable_processes_listening_on_ports_; }
  bool ImportUsers() const { return import_users_; }
  bool CollectConnectionStatus() const { return collect_connection_status_; }

  // EnableExternalIPs will check for the existence
  // of a runtime configuration, and defer to that value
  // otherwise, we rely on the feature flag (env var)
  bool EnableExternalIPs() const {
    if (runtime_config_.has_value()) {
      return runtime_config_.value()
          .networking()
          .external_ips()
          .enable();
    }
    return enable_external_ips_;
  }

  std::string GetRuntimeConfigStr() {
    if (runtime_config_.has_value()) {
      const auto& cfg = runtime_config_.value();
      return cfg.DebugString();
    }
    return "{}";
  }

  bool EnableConnectionStats() const { return enable_connection_stats_; }
  bool EnableDetailedMetrics() const { return enable_detailed_metrics_; }
  bool EnableRuntimeConfig() const { return enable_runtime_config_; }
  bool UseDockerCe() const { return use_docker_ce_; }
  bool UsePodmanCe() const { return use_podman_ce_; }
  bool IsIntrospectionEnabled() const { return enable_introspection_; }
  bool TrackingSendRecv() const { return track_send_recv_; }
  const std::vector<double>& GetConnectionStatsQuantiles() const { return connection_stats_quantiles_; }
  double GetConnectionStatsError() const { return connection_stats_error_; }
  unsigned int GetConnectionStatsWindow() const { return connection_stats_window_; }
  const std::optional<std::string>& GetGrpcServer() const { return grpc_server_; }
  unsigned int GetSinspCpuPerBuffer() const { return sinsp_cpu_per_buffer_; }
  unsigned int GetSinspBufferSize() const;
  unsigned int GetSinspTotalBufferSize() const { return sinsp_total_buffer_size_; }
  unsigned int GetSinspThreadCacheSize() const { return sinsp_thread_cache_size_; }

  static std::pair<option::ArgStatus, std::string> CheckConfiguration(const char* config, Json::Value* root);

  void SetRuntimeConfig(sensor::CollectorConfig&& runtime_config) {
    runtime_config_ = runtime_config;
  }

  void SetRuntimeConfig(sensor::CollectorConfig runtime_config) {
    runtime_config_ = std::move(runtime_config);
  }

  const std::optional<sensor::CollectorConfig>& GetRuntimeConfig() const {
    return runtime_config_;
  }

  std::shared_ptr<grpc::Channel> grpc_channel;

 private:
  int scrape_interval_;
  CollectionMethod collection_method_;
  bool turn_off_scrape_;
  std::vector<std::string> syscalls_;
  std::string hostname_;
  std::string host_proc_;
  bool disable_network_flows_ = false;
  bool scrape_listen_endpoints_ = false;
  UnorderedSet<L4ProtoPortPair> ignored_l4proto_port_pairs_;
  std::vector<IPNet> ignored_networks_;
  std::vector<IPNet> non_aggregated_networks_;

  HostConfig host_config_;
  int64_t afterglow_period_micros_ = 300'000'000;  // 5 minutes in microseconds
  bool enable_afterglow_ = false;
  bool enable_core_dump_ = false;
  bool enable_processes_listening_on_ports_;
  bool import_users_;
  bool collect_connection_status_;
  bool enable_external_ips_;
  bool enable_connection_stats_;
  bool enable_detailed_metrics_;
  bool enable_runtime_config_;
  bool use_docker_ce_;
  bool use_podman_ce_;
  bool enable_introspection_;
  bool track_send_recv_;
  std::vector<double> connection_stats_quantiles_;
  double connection_stats_error_;
  unsigned int connection_stats_window_;

  // URL to the GRPC server
  std::optional<std::string> grpc_server_;

  // One ring buffer will be initialized for this many CPUs
  unsigned int sinsp_cpu_per_buffer_ = 0;
  // Size of one ring buffer, in bytes.
  unsigned int sinsp_buffer_size_ = 0;
  // Allowed size of all ring buffers, in bytes. The default value 512Mi is
  // based on the default memory limit set of the Collector DaemonSet, which is
  // 1Gi.
  unsigned int sinsp_total_buffer_size_ = 512 * 1024 * 1024;

  // Max size of the thread cache. This parameter essentially translated into
  // the upper boundary for memory consumption. Note that Falco puts it's own
  // upper limit on top of this value, m_thread_table_absolute_max_size, which
  // is 2^17 (131072) and twice as large.
  unsigned int sinsp_thread_cache_size_ = 32768;

  std::optional<TlsConfig> tls_config_;

  std::optional<sensor::CollectorConfig> runtime_config_;

  void HandleAfterglowEnvVars();
  void HandleConnectionStatsEnvVars();
  void HandleSinspEnvVars();
  void YamlConfigToConfig(YAML::Node& yamlConfig);
  void HandleConfig(const std::filesystem::path& filePath);

  // Protected, used for testing purposes
  void SetSinspBufferSize(unsigned int buffer_size);
  void SetSinspTotalBufferSize(unsigned int total_buffer_size);
  void SetSinspCpuPerBuffer(unsigned int buffer_size);
  void SetHostConfig(HostConfig* config);

  void SetEnableExternalIPs(bool value) {
    enable_external_ips_ = value;
  }

  // Friend tests
  FRIEND_TEST(CollectorConfigTest, TestSinspBufferSizeReturnUnmodified);
  FRIEND_TEST(CollectorConfigTest, TestSinspCpuPerBufferAdjusted);
  FRIEND_TEST(CollectorConfigTest, TestEnableExternalIpsFeatureFlag);
  FRIEND_TEST(CollectorConfigTest, TestEnableExternalIpsRuntimeConfig);
  FRIEND_TEST(CollectorConfigTest, TestYamlConfigToConfigMultiple);
  FRIEND_TEST(CollectorConfigTest, TestYamlConfigToConfigInvalid);
  FRIEND_TEST(CollectorConfigTest, TestYamlConfigToConfigEmpty);
  FRIEND_TEST(NetworkStatusNotifier, UpdateIPnoAfterglow);
};

std::ostream& operator<<(std::ostream& os, const CollectorConfig& c);

}  // end namespace collector

#endif  // _COLLECTOR_CONFIG_H_
