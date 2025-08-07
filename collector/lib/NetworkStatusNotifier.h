#pragma once

#include <memory>
#include <utility>

#include <gtest/gtest_prod.h>

#include "CollectorConfig.h"
#include "CollectorConnectionStats.h"
#include "ConnTracker.h"
#include "NetworkConnectionInfoServiceComm.h"
#include "ProcfsScraper.h"
#include "ProtoAllocator.h"
#include "StoppableThread.h"

namespace collector {

class NetworkStatusNotifier : protected ProtoAllocator<sensor::NetworkConnectionInfoMessage> {
 public:
  NetworkStatusNotifier(std::shared_ptr<ConnectionTracker> conn_tracker,
                        const CollectorConfig& config,
                        system_inspector::Service* inspector,
                        prometheus::Registry* registry)
      : conn_scraper_(std::make_unique<ConnScraper>(config, inspector)),
        conn_tracker_(std::move(conn_tracker)),
        config_(config),
        comm_(std::make_unique<NetworkConnectionInfoServiceComm>(config.grpc_channel)) {
    if (config_.EnableConnectionStats()) {
      connections_total_reporter_ = {{registry,
                                      "rox_connections_total",
                                      "Amount of stored connections over time",
                                      std::chrono::minutes{config.GetConnectionStatsWindow()},
                                      config.GetConnectionStatsQuantiles(),
                                      config.GetConnectionStatsError()}};
      connections_rate_reporter_ = {{registry,
                                     "rox_connections_rate",
                                     "Rate of connections over time",
                                     std::chrono::minutes{config.GetConnectionStatsWindow()},
                                     config.GetConnectionStatsQuantiles(),
                                     config.GetConnectionStatsError()}};
    }
  }

  void Start();
  void Stop();

  /**
   * Replace the connection scraper object.
   *
   * This is meant to make testing easier by swapping in a mock object.
   *
   * @params cs A unique pointer to the new instance of the scraper to use.
   */
  void ReplaceConnScraper(std::unique_ptr<IConnScraper>&& cs) {
    conn_scraper_ = std::move(cs);
  }

  /**
   * Replace the communications object.
   *
   * This is meant to make testing easier by swapping in a mock object.
   *
   * @params comm A unique pointer to the new instance of communications
   *              to use.
   */
  void ReplaceComm(std::unique_ptr<INetworkConnectionInfoServiceComm>&& comm) {
    comm_ = std::move(comm);
  }

 private:
  FRIEND_TEST(NetworkStatusNotifierTest, RateLimitedConnections);

  sensor::NetworkConnectionInfoMessage* CreateInfoMessage(const ConnMap& conn_delta, const AdvertisedEndpointMap& cep_delta);
  void AddConnections(::google::protobuf::RepeatedPtrField<sensor::NetworkConnection>* updates, const ConnMap& delta);
  void AddContainerEndpoints(::google::protobuf::RepeatedPtrField<sensor::NetworkEndpoint>* updates, const AdvertisedEndpointMap& delta);

  sensor::NetworkConnection* ConnToProto(const Connection& conn);
  sensor::NetworkEndpoint* ContainerEndpointToProto(const ContainerEndpoint& cep);
  sensor::NetworkAddress* EndpointToProto(const Endpoint& endpoint);
  storage::NetworkProcessUniqueKey* ProcessToProto(const collector::IProcess& process);

  void OnRecvControlMessage(const sensor::NetworkFlowsControlMessage* msg);

  void Run();

  void WaitUntilWriterStarted(IDuplexClientWriter<sensor::NetworkConnectionInfoMessage>* writer, int wait_time);
  bool UpdateAllConnsAndEndpoints();
  void RunSingle(IDuplexClientWriter<sensor::NetworkConnectionInfoMessage>* writer);
  void ReceivePublicIPs(const sensor::IPAddressList& public_ips);
  void ReceiveIPNetworks(const sensor::IPNetworkList& networks);

  void ReportConnectionStats();

  StoppableThread thread_;

  std::unique_ptr<IConnScraper> conn_scraper_;
  std::shared_ptr<ConnectionTracker> conn_tracker_;

  const CollectorConfig& config_;
  std::unique_ptr<INetworkConnectionInfoServiceComm> comm_;

  std::optional<CollectorConnectionStats<unsigned int>> connections_total_reporter_;
  std::optional<CollectorConnectionStats<float>> connections_rate_reporter_;
  std::chrono::steady_clock::time_point connections_last_report_time_;     // time delta between the current reporting and the previous (rate computation)
  std::optional<ConnectionTracker::Stats> connections_rate_counter_last_;  // previous counter values (rate computation)
};

}  // namespace collector
