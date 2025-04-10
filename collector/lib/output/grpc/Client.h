#ifndef OUTPUT_GRPC_CLIENT
#define OUTPUT_GRPC_CLIENT

#include <grpcpp/channel.h>

#include "DuplexGRPC.h"
#include "output/IClient.h"

namespace collector::output::grpc {

using Channel = ::grpc::Channel;

class Client : public IClient {
 public:
  using Service = sensor::CollectorService;

  Client(const Client&) = delete;
  Client(Client&&) = delete;
  Client& operator=(const Client&) = delete;
  Client& operator=(Client&&) = delete;
  ~Client() override {
    context_->TryCancel();
  }

  explicit Client(std::shared_ptr<Channel> channel)
      : channel_(std::move(channel)) {
  }

  bool Recreate() override;

  SignalHandler::Result SendMsg(const sensor::ProcessSignal& msg) override;

 private:
  std::shared_ptr<Channel> channel_;

  std::atomic<bool> stream_active_ = false;

  // This needs to have the same lifetime as the class.
  std::unique_ptr<::grpc::ClientContext> context_;
  std::unique_ptr<IDuplexClientWriter<sensor::ProcessSignal>> writer_;

  bool first_write_ = false;
};
}  // namespace collector::output::grpc

#endif
