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
  stream_interrupted_.wait(lock, [this]() { return !stream_active_.load(std::memory_order_acquire) || stopping_.load(std::memory_order_acquire); });
  if (stopping_.load(std::memory_order_acquire)) {
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
  if (!stub_) {
    stub_ = SignalService::NewStub(channel_);
  }
  writer_ = std::make_unique<GrpcBidiStream<SignalStreamMessage, v1::Empty>>(
      context_.get(),
      [this](grpc::ClientContext* context, grpc::ClientBidiReactor<SignalStreamMessage, v1::Empty>* reactor) {
        stub_->async()->PushSignals(context, reactor);
      },
      nullptr);
  CLOG(INFO) << "Started GRPC call for signals.";

  first_write_ = true;
  stream_active_.store(true, std::memory_order_release);
  return true;
}

void SignalServiceClient::EstablishGRPCStream() {
  while (EstablishGRPCStreamSingle()) {
  }
  CLOG(INFO) << "Signal service client terminating.";
}

void SignalServiceClient::Start() {
  stopping_.store(false, std::memory_order_release);
  thread_.Start([this] { EstablishGRPCStream(); });
}

void SignalServiceClient::Stop() {
  stopping_.store(true, std::memory_order_release);
  stream_interrupted_.notify_one();
  thread_.Stop();
  if (context_) {
    context_->TryCancel();
  }
  writer_.reset();
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
    auto status = writer_->Finish(std::chrono::system_clock::now() + std::chrono::seconds(1));
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

SignalHandler::Result StdoutSignalServiceClient::PushSignals(const SignalStreamMessage& msg) {
  LogProtobufMessage(msg);
  return SignalHandler::PROCESSED;
}

}  // namespace collector
