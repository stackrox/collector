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

#include "ConnTracker.h"
#include "ProtoAllocator.h"
#include "StoppableThread.h"

namespace collector {

class NetworkStatusNotifier : protected ProtoAllocator<sensor::NetworkConnectionInfoMessage> {
 public:
  using Stub = sensor::NetworkConnectionInfoService::Stub;

  explicit NetworkStatusNotifier(std::shared_ptr<ConnectionTracker> conn_tracker)
      : conn_tracker_(std::move(conn_tracker)) {}

  void Run();

  void Start();
  void Stop();

 private:
  NetworkConnectionInfoMessage* CreateMessage(const ConnMap& conn_delta);

  bool RunSingle(grpc::ClientWriter<sensor::NetworkConnectionInfoMessage>* writer);

  grpc::ClientContext context_;

  StoppableThread thread_;

  std::unique_ptr<Stub> stub_;
  std::unique_ptr<grpc::ClientWriter<sensor::NetworkConnectionInfoMessage>> writer_;

  std::shared_ptr<ConnectionTracker> conn_tracker_;
};

}  // namespace collector

#endif //COLLECTOR_NETWORKSTATUSNOTIFIER_H
