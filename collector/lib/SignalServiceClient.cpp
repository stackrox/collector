#include "SignalServiceClient.h"

#include "Logging.h"
#include "Utility.h"

namespace collector {

bool SignalServiceClient::Recreate() {
  context_ = std::make_unique<grpc::ClientContext>();
  writer_ = DuplexClient::CreateWithReadsIgnored(&SignalService::Stub::AsyncPushSignals, channel_, context_.get());
  if (!writer_->WaitUntilStarted(std::chrono::seconds(30))) {
    CLOG(ERROR) << "Signal stream not ready after 30 seconds. Retrying ...";
    CLOG(ERROR) << "Error message: " << writer_->FinishNow().error_message();
    writer_.reset();
    return false;
  }

  first_write_ = true;
  stream_active_.store(true, std::memory_order_release);
  return true;
}

SignalHandler::Result SignalServiceClient::PushSignals(const SignalStreamMessage& msg) {
  if (!stream_active_.load(std::memory_order_acquire)) {
    CLOG_THROTTLED(ERROR, std::chrono::seconds(10))
        << "GRPC stream is not established";
    return SignalHandler::ERROR;
  }

  if (first_write_) {
    first_write_ = false;
    return SignalHandler::NEEDS_REFRESH;
  }

  if (!writer_->Write(msg)) {
    auto status = writer_->FinishNow();
    if (!status.ok()) {
      CLOG(ERROR) << "GRPC writes failed: " << status.error_message();
    }
    writer_.reset();
    stream_active_.store(false, std::memory_order_release);
    return SignalHandler::ERROR;
  }

  return SignalHandler::PROCESSED;
}

SignalHandler::Result StdoutSignalServiceClient::PushSignals(const SignalStreamMessage& msg) {
  LogProtobufMessage(msg);
  return SignalHandler::PROCESSED;
}

}  // namespace collector
