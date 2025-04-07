#include "SensorClient.h"

#include "Logging.h"

namespace collector {
bool SensorClient::Recreate() {
  context_ = std::make_unique<grpc::ClientContext>();
  writer_ = DuplexClient::CreateWithReadsIgnored(&sensor::CollectorService::Stub::AsyncCommunicate, channel_, context_.get());
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

SignalHandler::Result SensorClient::SendMsg(const sensor::MsgFromCollector& msg) {
  if (!stream_active_.load(std::memory_order_acquire)) {
    CLOG_THROTTLED(ERROR, std::chrono::seconds(10))
        << "GRPC stream is not established";
    return SignalHandler::ERROR;
  }

  if (first_write_ && msg.has_process_signal()) {
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

    stream_active_.store(false, std::memory_order_release);
    return SignalHandler::ERROR;
  }

  return SignalHandler::PROCESSED;
}
}  // namespace collector
