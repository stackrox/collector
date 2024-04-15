#ifndef _COLLECTOR_CONFIG_H_
#define _COLLECTOR_CONFIG_H_

#include <ostream>
#include <vector>

#include <json/json.h>

#include <grpcpp/channel.h>

#include "CollectionMethod.h"
#include "HostConfig.h"
#include "NetworkConnection.h"

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
  bool CurlVerbose() const { return curl_verbose_; }
  bool EnableAfterglow() const { return enable_afterglow_; }
  bool IsCoreDumpEnabled() const;
  Json::Value TLSConfiguration() const { return tls_config_; }
  bool IsProcessesListeningOnPortsEnabled() const { return enable_processes_listening_on_ports_; }
  bool ImportUsers() const { return import_users_; }
  bool CollectConnectionStatus() const { return collect_connection_status_; }
  bool EnableExternalIPs() const { return enable_external_ips_; }
  bool EnableConnectionStats() const { return enable_connection_stats_; }
  bool EnableDetailedMetrics() const { return enable_detailed_metrics_; }
  bool EnableRuntimeFilters() const { return enable_runtime_filters_; }
  bool UseDockerCe() const { return use_docker_ce_; }
  bool UsePodmanCe() const { return use_podman_ce_; }
  bool IsIntrospectionEnabled() const { return enable_introspection_; }
  const std::vector<double>& GetConnectionStatsQuantiles() const { return connection_stats_quantiles_; }
  double GetConnectionStatsError() const { return connection_stats_error_; }
  unsigned int GetConnectionStatsWindow() const { return connection_stats_window_; }
  unsigned int GetSinspBufferSize() const { return sinsp_buffer_size_; }
  unsigned int GetSinspCpuPerBuffer() const;
  unsigned int GetSinspTotalBufferSize() const { return sinsp_total_buffer_size_; }
  unsigned int GetSinspThreadCacheSize() const { return sinsp_thread_cache_size_; }

  std::shared_ptr<grpc::Channel> grpc_channel;

 protected:
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
  bool curl_verbose_ = false;

  HostConfig host_config_;
  int64_t afterglow_period_micros_ = 300000000;  // 5 minutes in microseconds
  bool enable_afterglow_ = true;
  bool enable_core_dump_ = false;
  bool enable_processes_listening_on_ports_;
  bool import_users_;
  bool collect_connection_status_;
  bool enable_external_ips_;
  bool enable_connection_stats_;
  bool enable_detailed_metrics_;
  bool enable_runtime_filters_;
  bool use_docker_ce_;
  bool use_podman_ce_;
  bool enable_introspection_;
  std::vector<double> connection_stats_quantiles_;
  double connection_stats_error_;
  unsigned int connection_stats_window_;

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

  Json::Value tls_config_;

  void HandleAfterglowEnvVars();
  void HandleConnectionStatsEnvVars();
  void HandleSinspEnvVars();

  // Protected, used for testing purposes
  void SetSinspBufferSize(unsigned int buffer_size);
  void SetSinspTotalBufferSize(unsigned int total_buffer_size);
  void SetSinspCpuPerBuffer(unsigned int buffer_size);
  void SetHostConfig(HostConfig* config);
};

std::ostream& operator<<(std::ostream& os, const CollectorConfig& c);

}  // end namespace collector

#endif  // _COLLECTOR_CONFIG_H_
