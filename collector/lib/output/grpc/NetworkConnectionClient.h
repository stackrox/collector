#ifndef OUTPUT_GRPC_NETWORK_CONNECTION_CLIENT_H
#define OUTPUT_GRPC_NETWORK_CONNECTION_CLIENT_H

#include "internalapi/sensor/collector_iservice.grpc.pb.h"

#include "Channel.h"
#include "DuplexGRPC.h"
#include "IGrpcClient.h"
#include "SignalHandler.h"

namespace collector::output::grpc {

class NetworkConnectionClient : public IGrpcClient {
 public:
  NetworkConnectionClient(const NetworkConnectionClient&) = delete;
  NetworkConnectionClient(NetworkConnectionClient&&) = delete;
  NetworkConnectionClient& operator=(const NetworkConnectionClient&) = delete;
  NetworkConnectionClient& operator=(NetworkConnectionClient&&) = delete;
  ~NetworkConnectionClient() override {
    if (context_) {
      context_->TryCancel();
    }
  }

  NetworkConnectionClient(std::shared_ptr<GrpcChannel> channel, bool use_sensor_client)
      : channel_(std::move(channel)),
        use_sensor_client_(use_sensor_client) {}

  bool Recreate() override;

  SignalHandler::Result SendMsg(const sensor::NetworkConnectionInfoMessage& msg);

  Channel<sensor::NetworkFlowsControlMessage>& GetControlMessageChannel() {
    return network_control_channel_;
  }

 private:
  std::unique_ptr<::grpc::ClientContext> NewContext();
  std::unique_ptr<IDuplexClientWriter<sensor::NetworkConnectionInfo>> NewWriter();
  std::unique_ptr<IDuplexClientWriter<sensor::NetworkConnectionInfoMessage>> NewLegacyWriter();

  static constexpr char kHostnameMetadataKey[] = "rox-collector-hostname";
  static constexpr char kCapsMetadataKey[] = "rox-collector-capabilities";

  // Keep this updated with all capabilities supported. Format it as a comma-separated list with NO spaces.
  static constexpr char kSupportedCaps[] = "public-ips,network-graph-external-srcs";

  std::shared_ptr<GrpcChannel> channel_;
  std::unique_ptr<::grpc::ClientContext> context_;
  std::unique_ptr<IDuplexClientWriter<sensor::NetworkConnectionInfo>> writer_;
  std::unique_ptr<IDuplexClientWriter<sensor::NetworkConnectionInfoMessage>> legacy_writer_;

  // A channel used for receiving events from the network connection
  // client. It needs to be blocking, since the read side will
  // overwrite the buffer on every call.
  Channel<sensor::NetworkFlowsControlMessage> network_control_channel_;

  bool use_sensor_client_;

  // Synchronize sending messages and client recreation
  std::mutex mutex_;
};

}  // namespace collector::output::grpc

#endif
