#pragma once

// SIGNAL_SERVICE_CLIENT.h
// This class defines our GRPC client abstraction

#include <mutex>

#include <grpc/grpc.h>
#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>

#include "api/v1/signal.pb.h"
#include "internalapi/sensor/signal_iservice.grpc.pb.h"

#include "DuplexGRPC.h"
#include "SignalHandler.h"
#include "StoppableThread.h"

namespace collector {

class ISignalServiceClient {
 public:
  using SignalStreamMessage = sensor::SignalStreamMessage;

  virtual bool Refresh() = 0;
  virtual SignalHandler::Result PushSignals(const SignalStreamMessage& msg) = 0;

  virtual ~ISignalServiceClient() = default;
};

class SignalServiceClient : public ISignalServiceClient {
 public:
  using SignalService = sensor::SignalService;
  using SignalStreamMessage = sensor::SignalStreamMessage;

  SignalServiceClient(const SignalServiceClient&) = delete;
  SignalServiceClient(SignalServiceClient&&) = delete;
  SignalServiceClient& operator=(const SignalServiceClient&) = delete;
  SignalServiceClient& operator=(SignalServiceClient&&) = delete;
  ~SignalServiceClient() override {
    context_->TryCancel();
  }

  explicit SignalServiceClient(std::shared_ptr<grpc::Channel> channel)
      : channel_(std::move(channel)), stream_active_(false) {}

  bool Refresh() override;

  SignalHandler::Result PushSignals(const SignalStreamMessage& msg) override;

 private:
  void EstablishGRPCStream();
  bool EstablishGRPCStreamSingle();

  std::shared_ptr<grpc::Channel> channel_;

  std::atomic<bool> stream_active_;

  // This needs to have the same lifetime as the class.
  std::unique_ptr<grpc::ClientContext> context_;
  std::unique_ptr<IDuplexClientWriter<SignalStreamMessage>> writer_;

  bool first_write_{};
};

class StdoutSignalServiceClient : public ISignalServiceClient {
 public:
  using SignalStreamMessage = sensor::SignalStreamMessage;

  explicit StdoutSignalServiceClient() = default;

  bool Refresh() override { return true; }

  SignalHandler::Result PushSignals(const SignalStreamMessage& msg) override;
};

}  // namespace collector
