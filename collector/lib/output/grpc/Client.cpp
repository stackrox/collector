#include "Client.h"

#include <variant>

#include "GRPCUtil.h"
#include "Logging.h"
#include "SignalHandler.h"

namespace collector::output::grpc {

bool Client::Recreate() {
  if (use_sensor_client_) {
    if (!process_client_->Recreate()) {
      return false;
    }
  } else {
    if (!signal_client_->Recreate()) {
      return false;
    }
  }

  if (!network_connection_client_->Recreate()) {
    return false;
  }

  stream_active_.store(true, std::memory_order_release);
  client_ready_.notify_all();
  return true;
}

SignalHandler::Result Client::SendMsg(const MsgToSensor& msg) {
  if (!stream_active_.load(std::memory_order_acquire)) {
    CLOG_THROTTLED(ERROR, std::chrono::seconds(10))
        << "GRPC stream is not established";
    return SignalHandler::ERROR;
  }

  // Delegate to the proper SendMsg implementation depending on the type held.
  if (const auto* m = std::get_if<sensor::ProcessSignal>(&msg)) {
    auto res = process_client_->SendMsg(*m);
    if (res == SignalHandler::Result::ERROR) {
      stream_active_.store(false, std::memory_order_release);
      stream_interrupted_.notify_one();
    }
    return res;
  }

  if (const auto* m = std::get_if<sensor::SignalStreamMessage>(&msg)) {
    auto res = signal_client_->SendMsg(*m);
    if (res == SignalHandler::Result::ERROR) {
      stream_active_.store(false, std::memory_order_release);
      stream_interrupted_.notify_one();
    }
    return res;
  }

  if (const auto* m = std::get_if<sensor::NetworkConnectionInfoMessage>(&msg)) {
    auto res = network_connection_client_->SendMsg(*m);
    if (res == SignalHandler::Result::ERROR) {
      stream_active_.store(false, std::memory_order_release);
      stream_interrupted_.notify_one();
    }
    return res;
  }

  CLOG(ERROR) << "Unknown type in MsgToSensor variant";
  return SignalHandler::Result::ERROR;
}

void Client::EstablishGrpcStream() {
  while (EstablishGrpcStreamSingle()) {
  }
  CLOG(INFO) << "Service client terminating.";
}

bool Client::EstablishGrpcStreamSingle() {
  std::mutex mtx;
  std::unique_lock<std::mutex> lock(mtx);
  stream_interrupted_.wait(lock, [this]() { return !stream_active_.load(std::memory_order_acquire) || thread_.should_stop(); });
  if (thread_.should_stop()) {
    return false;
  }

  CLOG(INFO) << "Trying to establish GRPC stream...";

  if (!WaitForChannelReady(channel_, [this]() { return thread_.should_stop(); })) {
    return false;
  }
  if (thread_.should_stop()) {
    return false;
  }

  // Refresh all clients
  if (Recreate()) {
    CLOG(INFO) << "Successfully established GRPC stream.";
  } else {
    CLOG(WARNING) << "Failed to establish GRPC stream, retrying...";
  }
  return true;
}
}  // namespace collector::output::grpc
