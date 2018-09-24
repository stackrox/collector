//
// Created by Malte Isberner on 9/18/18.
//

#ifndef COLLECTOR_NETWORKSTATUSNOTIFIER_H
#define COLLECTOR_NETWORKSTATUSNOTIFIER_H

#include <memory>

#include <grpc/grpc.h>
#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>

#include "internalapi/sensor/network_connection_service.grpc.pb.h"

#include "ConnScraper.h"
#include "ConnTracker.h"
#include "DuplexGRPC.h"
#include "ProtoAllocator.h"
#include "StoppableThread.h"

namespace collector {

class NetworkStatusNotifier : protected ProtoAllocator<sensor::NetworkConnectionInfoMessage> {
 public:
  using Stub = sensor::NetworkConnectionInfoService::Stub;

  NetworkStatusNotifier(std::string hostname, std::string proc_dir, std::shared_ptr<ConnectionTracker> conn_tracker,
                        std::shared_ptr<grpc::Channel> channel)
      : hostname_(std::move(hostname)), conn_scraper_(std::move(proc_dir)), conn_tracker_(std::move(conn_tracker)),
        channel_(std::move(channel)), stub_(sensor::NetworkConnectionInfoService::NewStub(channel_))
  {}

  void Start();
  void Stop();

 private:
  sensor::NetworkConnectionInfoMessage* CreateInfoMessage(const ConnMap& conn_delta);
  sensor::NetworkConnection* ConnToProto(const Connection& conn);
  sensor::NetworkAddress* EndpointToProto(const Endpoint& endpoint);

  void Run();
  void RunSingle(DuplexClientWriter<sensor::NetworkConnectionInfoMessage>* writer);

  std::string hostname_;

  StoppableThread thread_;

  std::unique_ptr<grpc::ClientContext> context_;
  std::mutex context_mutex_;

  ConnScraper conn_scraper_;
  std::shared_ptr<ConnectionTracker> conn_tracker_;

  std::shared_ptr<grpc::Channel> channel_;
  std::unique_ptr<Stub> stub_;
};

}  // namespace collector

#endif //COLLECTOR_NETWORKSTATUSNOTIFIER_H
