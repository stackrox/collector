#include "SensorClient.h"

#include "GRPCUtil.h"
#include "Logging.h"

namespace collector {
bool SensorClient::EstablishGRPCStreamSingle() {
  std::mutex mtx;
  std::unique_lock<std::mutex> lock(mtx);
  stream_interrupted_.wait(lock, [this]() { return !stream_active_.load(std::memory_order_acquire) || thread_.should_stop(); });
  if (thread_.should_stop()) {
    return false;
  }

  CLOG(INFO) << "Trying to establish GRPC stream for signals ...";

  if (!WaitForChannelReady(channel_, [this]() { return thread_.should_stop(); })) {
    return false;
  }
  if (thread_.should_stop()) {
    return false;
  }

  // stream writer
  context_ = std::make_unique<grpc::ClientContext>();
  writer_ = DuplexClient::CreateWithReadsIgnored(&sensor::CollectorService::Stub::AsyncCommunicate, channel_, context_.get());
  if (!writer_->WaitUntilStarted(std::chrono::seconds(30))) {
    CLOG(ERROR) << "Signal stream not ready after 30 seconds. Retrying ...";
    CLOG(ERROR) << "Error message: " << writer_->FinishNow().error_message();
    writer_.reset();
    return true;
  }
  CLOG(INFO) << "Successfully established GRPC stream for signals.";

  first_write_ = true;
  stream_active_.store(true, std::memory_order_release);
  return true;
}

void SensorClient::EstablishGRPCStream() {
  while (EstablishGRPCStreamSingle());
  CLOG(INFO) << "Signal service client terminating.";
}

void SensorClient::Start() {
  thread_.Start([this] { EstablishGRPCStream(); });
}

void SensorClient::Stop() {
  stream_interrupted_.notify_one();
  thread_.Stop();
  context_->TryCancel();
  context_.reset();
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

  if (!writer_->Write(msg)) {
    auto status = writer_->FinishNow();
    if (!status.ok()) {
      CLOG(ERROR) << "GRPC writes failed: (" << status.error_code() << ") " << status.error_message();
    }
    writer_.reset();

    stream_active_.store(false, std::memory_order_release);
    CLOG(ERROR) << "GRPC stream interrupted";
    stream_interrupted_.notify_one();
    return SignalHandler::ERROR;
  }

  return SignalHandler::PROCESSED;
}
}  // namespace collector
