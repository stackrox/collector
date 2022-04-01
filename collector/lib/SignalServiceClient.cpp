#include "SignalServiceClient.h"

#include <fstream>

#include "GRPCUtil.h"
#include "Logging.h"
#include "ProtoUtil.h"
#include "Utility.h"

namespace collector {

bool SignalServiceClient::EstablishGRPCStreamSingle() {
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
  context_ = MakeUnique<grpc::ClientContext>();
  writer_ = DuplexClient::CreateWithReadsIgnored(&SignalService::Stub::AsyncPushSignals, channel_, context_.get());
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

void SignalServiceClient::EstablishGRPCStream() {
  while (EstablishGRPCStreamSingle())
    ;
  CLOG(INFO) << "Signal service client terminating.";
}

void SignalServiceClient::Start() {
  thread_.Start([this] { EstablishGRPCStream(); });
}

void SignalServiceClient::Stop() {
  stream_interrupted_.notify_one();
  thread_.Stop();
  context_->TryCancel();
  context_.reset();
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
    CLOG(ERROR) << "GRPC stream interrupted";
    stream_interrupted_.notify_one();
    return SignalHandler::ERROR;
  }

  return SignalHandler::PROCESSED;
}

}  // namespace collector
