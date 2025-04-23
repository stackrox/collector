#pragma once

#include <memory>
#include <utility>

#include <gtest/gtest_prod.h>

#include "CollectorConfig.h"
#include "CollectorConnectionStats.h"
#include "ConnTracker.h"
#include "DuplexGRPC.h"
#include "ProcfsScraper.h"
#include "ProtoAllocator.h"
#include "StoppableThread.h"
#include "output/Output.h"

namespace collector {

class NetworkStatusNotifier : protected ProtoAllocator<sensor::NetworkConnectionInfoMessage> {
 public:
  NetworkStatusNotifier(std::shared_ptr<ConnectionTracker> conn_tracker,
                        const CollectorConfig& config,
                        system_inspector::Service* inspector,
                        output::Output* output,
                        prometheus::Registry* registry)
      : conn_scraper_(std::make_unique<ConnScraper>(config, inspector)),
        conn_tracker_(std::move(conn_tracker)),
        config_(config),
        output_(output),
        receiver_(output_->GetNetworkControlChannel(), [this](const auto& msg) { OnRecvControlMessage(msg); }) {
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

 private:
  FRIEND_TEST(NetworkStatusNotifierTest, SimpleStartStop);
  FRIEND_TEST(NetworkStatusNotifierTest, RateLimitedConnections);

  sensor::NetworkConnectionInfoMessage* CreateInfoMessage(const ConnMap& conn_delta, const AdvertisedEndpointMap& cep_delta);
  void AddConnections(::google::protobuf::RepeatedPtrField<sensor::NetworkConnection>* updates, const ConnMap& delta);
  void AddContainerEndpoints(::google::protobuf::RepeatedPtrField<sensor::NetworkEndpoint>* updates, const AdvertisedEndpointMap& delta);

  sensor::NetworkConnection* ConnToProto(const Connection& conn_proto);
  sensor::NetworkEndpoint* ContainerEndpointToProto(const ContainerEndpoint& cep);
  sensor::NetworkAddress* EndpointToProto(const Endpoint& endpoint);
  storage::NetworkProcessUniqueKey* ProcessToProto(const collector::IProcess& process);

  void OnRecvControlMessage(const sensor::NetworkFlowsControlMessage& msg);

  void Run();
  void WaitUntilWriterStarted(IDuplexClientWriter<sensor::NetworkConnectionInfoMessage>* writer, int wait_time);
  bool UpdateAllConnsAndEndpoints();
  void RunSingle();
  void ReceivePublicIPs(const sensor::IPAddressList& public_ips);
  void ReceiveIPNetworks(const sensor::IPNetworkList& networks);

  void ReportConnectionStats();

  StoppableThread thread_;

  std::unique_ptr<IConnScraper> conn_scraper_;
  std::shared_ptr<ConnectionTracker> conn_tracker_;

  const CollectorConfig& config_;
  output::Output* output_;

  class Receiver {
   public:
    using callback_t = std::function<void(const sensor::NetworkFlowsControlMessage&)>;
    Receiver(Channel<sensor::NetworkFlowsControlMessage>& channel, callback_t callback)
        : channel_(channel), callback_(std::move(callback)) {}

    void Start() {
      thread_.Start([this] {
        sensor::NetworkFlowsControlMessage ctrl;
        while (!thread_.should_stop()) {
          for (const auto& ctrl : channel_) {
            callback_(ctrl);
          }
        }
      });
    }

    void Stop() {
      channel_.Close();
      thread_.Stop();
    }

   private:
    StoppableThread thread_;
    Channel<sensor::NetworkFlowsControlMessage>& channel_;
    std::mutex mutex_;
    std::condition_variable cv_;

    std::function<void(const sensor::NetworkFlowsControlMessage&)> callback_;
  } receiver_;

  std::optional<CollectorConnectionStats<unsigned int>> connections_total_reporter_;
  std::optional<CollectorConnectionStats<float>> connections_rate_reporter_;
  std::chrono::steady_clock::time_point connections_last_report_time_;     // time delta between the current reporting and the previous (rate computation)
  std::optional<ConnectionTracker::Stats> connections_rate_counter_last_;  // previous counter values (rate computation)
};

}  // namespace collector
