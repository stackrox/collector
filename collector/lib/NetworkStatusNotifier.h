#ifndef COLLECTOR_NETWORKSTATUSNOTIFIER_H
#define COLLECTOR_NETWORKSTATUSNOTIFIER_H

#include <memory>

#include "CollectorStats.h"
#include "ConnTracker.h"
#include "NetworkConnectionInfoServiceComm.h"
#include "ProcfsScraper.h"
#include "ProtoAllocator.h"
#include "StoppableThread.h"

namespace collector {

class NetworkStatusNotifier : protected ProtoAllocator<sensor::NetworkConnectionInfoMessage> {
 public:
  NetworkStatusNotifier(std::shared_ptr<IConnScraper> conn_scraper, int scrape_interval, bool scrape_listen_endpoints, bool turn_off_scrape,
                        std::shared_ptr<ConnectionTracker> conn_tracker, int64_t afterglow_period_micros, bool use_afterglow,
                        std::shared_ptr<INetworkConnectionInfoServiceComm> comm)
      : conn_scraper_(conn_scraper), scrape_interval_(scrape_interval), turn_off_scraping_(turn_off_scrape), scrape_listen_endpoints_(scrape_listen_endpoints), conn_tracker_(std::move(conn_tracker)), afterglow_period_micros_(afterglow_period_micros), enable_afterglow_(use_afterglow), comm_(comm) {
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
  void WaitUntilWriterStarted(IDuplexClientWriter<sensor::NetworkConnectionInfoMessage>* writer, int wait_time);
  bool UpdateAllConnsAndEndpoints();
  void RunSingle(IDuplexClientWriter<sensor::NetworkConnectionInfoMessage>* writer);
  void RunSingleAfterglow(IDuplexClientWriter<sensor::NetworkConnectionInfoMessage>* writer);
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
  std::shared_ptr<INetworkConnectionInfoServiceComm> comm_;
};

}  // namespace collector

#endif  // COLLECTOR_NETWORKSTATUSNOTIFIER_H
