#ifndef __SIGNAL_SERVICE_CLIENT_H
#define __SIGNAL_SERVICE_CLIENT_H

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

  virtual void Start() = 0;
  virtual void Stop() = 0;
  virtual SignalHandler::Result PushSignals(const SignalStreamMessage& msg) = 0;

  virtual ~ISignalServiceClient() {}
};

class SignalServiceClient : public ISignalServiceClient {
 public:
  using SignalService = sensor::SignalService;
  using SignalStreamMessage = sensor::SignalStreamMessage;

  explicit SignalServiceClient(std::shared_ptr<grpc::Channel> channel)
      : channel_(std::move(channel)), stream_active_(false) {}

  void Start();
  void Stop();

  SignalHandler::Result PushSignals(const SignalStreamMessage& msg);

 private:
  void EstablishGRPCStream();
  bool EstablishGRPCStreamSingle();

  std::shared_ptr<grpc::Channel> channel_;

  StoppableThread thread_;
  std::atomic<bool> stream_active_;
  std::condition_variable stream_interrupted_;

  // This needs to have the same lifetime as the class.
  std::unique_ptr<grpc::ClientContext> context_;
  std::unique_ptr<IDuplexClientWriter<SignalStreamMessage>> writer_;

  bool first_write_;
};

class StdoutSignalServiceClient : public ISignalServiceClient {
 public:
  using SignalStreamMessage = sensor::SignalStreamMessage;

  explicit StdoutSignalServiceClient() {}

  void Start(){};
  void Stop(){};

  SignalHandler::Result PushSignals(const SignalStreamMessage& msg);
};

}  // namespace collector

#endif  // __SIGNAL_SERVICE_CLIENT_H
