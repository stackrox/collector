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
  static constexpr CollectionMethod kCollectionMethod = CollectionMethod::EBPF;
  static constexpr const char* kSyscalls[] = {
      "accept",
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

  CollectorConfig() = delete;
  CollectorConfig(CollectorArgs* collectorArgs);

  std::string asString() const;

  void HandleAfterglowEnvVars();
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
  bool CurlVerbose() const { return curl_verbose_; }
  bool EnableAfterglow() const { return enable_afterglow_; }
  bool IsCoreDumpEnabled() const;
  Json::Value TLSConfiguration() const { return tls_config_; }
  bool IsProcessesListeningOnPortsEnabled() const { return enable_processes_listening_on_ports_; }
  bool CoReBPFHardfail() const { return core_bpf_hardfail_; }
  bool ImportUsers() const { return import_users_; }
  bool CollectConnectionStatus() const { return collect_connection_status_; }
  bool EnableExternalIPs() const { return enable_external_ips_; }
  bool EnableConnectionStats() const { return enable_connection_stats_; }
  const std::vector<double>& GetConnectionStatsQuantiles() const { return connection_stats_quantiles_; }
  double GetConnectionStatsError() const { return connection_stats_error_; }
  unsigned int GetConnectionStatsWindow() const { return connection_stats_window_; }

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
  bool curl_verbose_ = false;

  HostConfig host_config_;
  int64_t afterglow_period_micros_ = 300000000;  // 5 minutes in microseconds
  bool enable_afterglow_ = true;
  bool enable_core_dump_ = false;
  bool enable_processes_listening_on_ports_;
  bool core_bpf_hardfail_;
  bool import_users_;
  bool collect_connection_status_;
  bool enable_external_ips_;
  bool enable_connection_stats_;
  std::vector<double> connection_stats_quantiles_;
  double connection_stats_error_;
  unsigned int connection_stats_window_;

  Json::Value tls_config_;
};

std::ostream& operator<<(std::ostream& os, const CollectorConfig& c);

}  // end namespace collector

#endif  // _COLLECTOR_CONFIG_H_
