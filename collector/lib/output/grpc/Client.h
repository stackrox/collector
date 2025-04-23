#ifndef OUTPUT_GRPC_CLIENT
#define OUTPUT_GRPC_CLIENT

#include <grpcpp/channel.h>

#include "Channel.h"
#include "StoppableThread.h"
#include "output/IClient.h"
#include "output/grpc/IGrpcClient.h"
#include "output/grpc/NetworkConnectionClient.h"
#include "output/grpc/ProcessClient.h"
#include "output/grpc/SignalServiceClient.h"

namespace collector::output::grpc {

class Client : public IClient {
 public:
  using Service = sensor::CollectorService;

  Client(const Client&) = delete;
  Client(Client&&) = delete;
  Client& operator=(const Client&) = delete;
  Client& operator=(Client&&) = delete;
  ~Client() override {
    stream_interrupted_.notify_one();
    thread_.Stop();
  }

  explicit Client(std::shared_ptr<GrpcChannel> channel, std::condition_variable& client_ready, bool use_sensor_client)
      : client_ready_(client_ready), channel_(std::move(channel)), use_sensor_client_(use_sensor_client) {
    if (use_sensor_client_) {
      process_client_ = std::make_unique<ProcessClient>(channel_);
    } else {
      signal_client_ = std::make_unique<SignalServiceClient>(channel_);
    }
    network_connection_client_ = std::make_unique<NetworkConnectionClient>(channel_, use_sensor_client_);

    thread_.Start([this] { EstablishGrpcStream(); });
  }

  bool Recreate();
  SignalHandler::Result SendMsg(const MsgToSensor& msg) override;
  bool IsReady() override { return stream_active_.load(std::memory_order_acquire); }

  Channel<sensor::NetworkFlowsControlMessage>& GetControlMessageChannel() {
    return network_connection_client_->GetControlMessageChannel();
  }

 private:
  void EstablishGrpcStream();
  bool EstablishGrpcStreamSingle();

  // New clients
  std::unique_ptr<ProcessClient> process_client_;
  std::unique_ptr<NetworkConnectionClient> network_connection_client_;

  // Legacy clients
  std::unique_ptr<SignalServiceClient> signal_client_;

  std::atomic<bool> stream_active_ = false;
  std::condition_variable stream_interrupted_;
  std::condition_variable& client_ready_;
  std::shared_ptr<GrpcChannel> channel_;
  StoppableThread thread_;

  bool use_sensor_client_;
};
}  // namespace collector::output::grpc

#endif
