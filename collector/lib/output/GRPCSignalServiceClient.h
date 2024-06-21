#ifndef _COLLECTOR_GRPC_SIGNAL_SVC_CLIENT
#define _COLLECTOR_GRPC_SIGNAL_SVC_CLIENT

#include <grpc/grpc.h>
#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>

#include "api/v1/signal.pb.h"
#include "internalapi/sensor/signal_iservice.grpc.pb.h"

#include "DuplexGRPC.h"
#include "OutputClient.h"
#include "SignalHandler.h"
#include "StoppableThread.h"

namespace collector::output {

class GRPCSignalServiceClient : public OutputClient {
 public:
  using SignalService = sensor::SignalService;

  explicit GRPCSignalServiceClient(std::shared_ptr<grpc::Channel> channel)
      : channel_(std::move(channel)), stream_active_(false) {}

  void Start();
  void Stop();
  bool Ready();

  bool PushSignals(const OutputClient::Message& msg);

 private:
  void EstablishGRPCStream();
  bool EstablishGRPCStreamSingle();

  std::shared_ptr<grpc::Channel> channel_;

  StoppableThread thread_;
  std::atomic<bool> stream_active_;
  std::condition_variable stream_interrupted_;

  // This needs to have the same lifetime as the class.
  std::unique_ptr<grpc::ClientContext> context_;
  std::unique_ptr<IDuplexClientWriter<OutputClient::Message>> writer_;

  bool first_write_;
};

};  // namespace collector::output

#endif
