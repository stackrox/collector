#ifndef COLLECTOR_NETWORKCONNECTIONINFOSERVICECOMM_H
#define COLLECTOR_NETWORKCONNECTIONINFOSERVICECOMM_H

#include <memory>

#include <grpc/grpc.h>
#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>

#include "internalapi/sensor/network_connection_iservice.grpc.pb.h"

#include "DuplexGRPC.h"

namespace collector {

// Gathers all the communication routines targeted at NetworkConnectionInfoService.
// A simple gRPC mock is not sufficient for testing, since it doesn't abstract Streams.
class INetworkConnectionInfoServiceComm {
 public:
  virtual ~INetworkConnectionInfoServiceComm() {}

  virtual void ResetClientContext() = 0;
  // return false on failure
  virtual bool WaitForConnectionReady(const std::function<bool()>& check_interrupted) = 0;
  virtual void TryCancel() = 0;

  virtual sensor::NetworkConnectionInfoService::StubInterface* GetStub() = 0;

  virtual std::unique_ptr<IDuplexClientWriter<sensor::NetworkConnectionInfoMessage>> PushNetworkConnectionInfoOpenStream(std::function<void(const sensor::NetworkFlowsControlMessage*)> receive_func) = 0;
};

class NetworkConnectionInfoServiceComm : public INetworkConnectionInfoServiceComm {
 public:
  NetworkConnectionInfoServiceComm(std::string hostname, std::shared_ptr<grpc::Channel> channel);

  void ResetClientContext() override;
  bool WaitForConnectionReady(const std::function<bool()>& check_interrupted) override;
  void TryCancel() override;

  sensor::NetworkConnectionInfoService::StubInterface* GetStub() override {
    return stub_.get();
  }

  std::unique_ptr<IDuplexClientWriter<sensor::NetworkConnectionInfoMessage>> PushNetworkConnectionInfoOpenStream(std::function<void(const sensor::NetworkFlowsControlMessage*)> receive_func) override;

 private:
  static constexpr char kHostnameMetadataKey[] = "rox-collector-hostname";
  static constexpr char kCapsMetadataKey[] = "rox-collector-capabilities";

  // Keep this updated with all capabilities supported. Format it as a comma-separated list with NO spaces.
  static constexpr char kSupportedCaps[] = "public-ips,network-graph-external-srcs";

  std::unique_ptr<grpc::ClientContext> CreateClientContext() const;

  std::string hostname_;
  std::shared_ptr<grpc::Channel> channel_;
  std::unique_ptr<sensor::NetworkConnectionInfoService::Stub> stub_;

  std::mutex context_mutex_;
  std::unique_ptr<grpc::ClientContext> context_;
};

}  // namespace collector

#endif
