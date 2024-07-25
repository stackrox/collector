#ifndef COLLECTOR_NETWORKSTATUSNOTIFIER_H
#define COLLECTOR_NETWORKSTATUSNOTIFIER_H

#include <memory>

#include "CollectorConfig.h"
#include "CollectorStats.h"
#include "ConnTracker.h"
#include "ProcfsScraper.h"
#include "ProtoAllocator.h"
#include "StoppableThread.h"
#include "output/NetworkConnectionInfoServiceComm.h"

namespace collector {

class NetworkStatusNotifier : protected ProtoAllocator<sensor::NetworkConnectionInfoMessage> {
 public:
  NetworkStatusNotifier(std::shared_ptr<IConnScraper> conn_scraper,
                        std::shared_ptr<ConnectionTracker> conn_tracker,
                        std::shared_ptr<output::INetworkConnectionInfoServiceComm> comm,
                        const CollectorConfig& config,
                        std::shared_ptr<CollectorConnectionStats<unsigned int>> connections_total_reporter = 0,
                        std::shared_ptr<CollectorConnectionStats<float>> connections_rate_reporter = 0)
      : conn_scraper_(conn_scraper),
        scrape_interval_(config.ScrapeInterval()),
        turn_off_scraping_(config.TurnOffScrape()),
        scrape_listen_endpoints_(config.ScrapeListenEndpoints()),
        conn_tracker_(std::move(conn_tracker)),
        afterglow_period_micros_(config.AfterglowPeriod()),
        enable_afterglow_(config.EnableAfterglow()),
        comm_(comm),
        connections_total_reporter_(connections_total_reporter),
        connections_rate_reporter_(connections_rate_reporter) {
  }

  void Start();
  void Stop();

 private:
  sensor::NetworkConnectionInfoMessage* CreateInfoMessage(const ConnMap& conn_delta, const AdvertisedEndpointMap& cep_delta);
  void AddConnections(::google::protobuf::RepeatedPtrField<sensor::NetworkConnection>* updates, const ConnMap& delta);
  void AddContainerEndpoints(::google::protobuf::RepeatedPtrField<sensor::NetworkEndpoint>* updates, const AdvertisedEndpointMap& delta);

  sensor::NetworkConnection* ConnToProto(const Connection& conn);
  sensor::NetworkEndpoint* ContainerEndpointToProto(const ContainerEndpoint& cep);
  sensor::NetworkAddress* EndpointToProto(const Endpoint& endpoint);
  storage::NetworkProcessUniqueKey* ProcessToProto(const collector::IProcess& process);

  void OnRecvControlMessage(const sensor::NetworkFlowsControlMessage* msg);

  void Run();
  void WaitUntilWriterStarted(output::IDuplexClientWriter<sensor::NetworkConnectionInfoMessage>* writer, int wait_time);
  bool UpdateAllConnsAndEndpoints();
  void RunSingle(output::IDuplexClientWriter<sensor::NetworkConnectionInfoMessage>* writer);
  void RunSingleAfterglow(output::IDuplexClientWriter<sensor::NetworkConnectionInfoMessage>* writer);
  void ReceivePublicIPs(const sensor::IPAddressList& public_ips);
  void ReceiveIPNetworks(const sensor::IPNetworkList& networks);

  StoppableThread thread_;

  std::shared_ptr<IConnScraper> conn_scraper_;
  int scrape_interval_;
  bool turn_off_scraping_;
  bool scrape_listen_endpoints_;
  std::shared_ptr<ConnectionTracker> conn_tracker_;

  int64_t afterglow_period_micros_;
  bool enable_afterglow_;
  std::shared_ptr<output::INetworkConnectionInfoServiceComm> comm_;

  std::shared_ptr<CollectorConnectionStats<unsigned int>> connections_total_reporter_;
  std::shared_ptr<CollectorConnectionStats<float>> connections_rate_reporter_;
  std::chrono::steady_clock::time_point connections_last_report_time_;     // time delta between the current reporting and the previous (rate computation)
  std::optional<ConnectionTracker::Stats> connections_rate_counter_last_;  // previous counter values (rate computation)
  void ReportConnectionStats();
};

}  // namespace collector

#endif  // COLLECTOR_NETWORKSTATUSNOTIFIER_H
