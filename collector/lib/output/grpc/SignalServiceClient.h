#ifndef OUTPUT_GRPC_SIGNAL_SERVICE_CLIENT
#define OUTPUT_GRPC_SIGNAL_SERVICE_CLIENT

#include "internalapi/sensor/signal_iservice.grpc.pb.h"

#include "DuplexGRPC.h"
#include "IGrpcClient.h"
#include "Logging.h"
#include "SignalHandler.h"

namespace collector::output::grpc {

class SignalServiceClient : public IGrpcClient {
 public:
  SignalServiceClient(const SignalServiceClient&) = delete;
  SignalServiceClient(SignalServiceClient&&) = delete;
  SignalServiceClient& operator=(const SignalServiceClient&) = delete;
  SignalServiceClient& operator=(SignalServiceClient&&) = delete;
  ~SignalServiceClient() override {
    if (context_) {
      context_->TryCancel();
    }
  }

  SignalServiceClient(std::shared_ptr<GrpcChannel> channel)
      : channel_(std::move(channel)) {}

  bool Recreate() override {
    std::unique_lock<std::mutex> l{mutex_};

    writer_.reset();
    context_ = std::make_unique<::grpc::ClientContext>();
    writer_ = DuplexClient::CreateWithReadsIgnored(&sensor::SignalService::Stub::AsyncPushSignals, channel_, context_.get());

    if (!writer_->WaitUntilStarted(std::chrono::seconds(30))) {
      CLOG(ERROR) << "Process stream not ready after 30 seconds. Retrying ...";
      CLOG(ERROR) << "Error message: " << writer_->FinishNow().error_message();
      writer_.reset();
      return false;
    }
    return true;
  }

  SignalHandler::Result SendMsg(const sensor::SignalStreamMessage& msg) {
    std::unique_lock<std::mutex> l{mutex_};

    if (first_write_) {
      first_write_ = false;
      return SignalHandler::NEEDS_REFRESH;
    }

    auto res = writer_->Write(msg);
    if (!res) {
      auto status = writer_->FinishNow();
      if (!status.ok()) {
        CLOG(ERROR) << "GRPC writes failed: (" << status.error_code() << ") " << status.error_message();
      }
      writer_.reset();
      return SignalHandler::ERROR;
    }

    return SignalHandler::PROCESSED;
  }

 private:
  bool first_write_ = false;

  std::shared_ptr<GrpcChannel> channel_;
  std::unique_ptr<::grpc::ClientContext> context_;
  std::unique_ptr<IDuplexClientWriter<sensor::SignalStreamMessage>> writer_;

  // Synchronize sending messages and client recreation
  std::mutex mutex_;
};

}  // namespace collector::output::grpc

#endif
