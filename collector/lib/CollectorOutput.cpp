#include "CollectorOutput.h"

#include "internalapi/sensor/collector_iservice.pb.h"

#include "GRPCUtil.h"
#include "HostInfo.h"

namespace collector {

CollectorOutput::CollectorOutput(const CollectorConfig& config)
    : use_sensor_client_(!config.UseLegacyServices()) {
  if (config.grpc_channel != nullptr) {
    channel_ = config.grpc_channel;

    if (use_sensor_client_) {
      auto sensor_client = std::make_unique<SensorClient>(channel_);
      sensor_clients_.emplace_back(std::move(sensor_client));
    } else {
      auto signal_client = std::make_unique<SignalServiceClient>(channel_);
      signal_clients_.emplace_back(std::move(signal_client));
    }
  }

  if (config.grpc_channel == nullptr || config.UseStdout()) {
    if (use_sensor_client_) {
      auto sensor_client = std::make_unique<SensorClientStdout>();
      sensor_clients_.emplace_back(std::move(sensor_client));
    } else {
      auto signal_client = std::make_unique<StdoutSignalServiceClient>();
      signal_clients_.emplace_back(std::move(signal_client));
    }
  }

  thread_.Start([this] { EstablishGrpcStream(); });
}

void CollectorOutput::HandleOutputError() {
  CLOG(ERROR) << "GRPC stream interrupted";
  stream_active_.store(false, std::memory_order_release);
  stream_interrupted_.notify_one();
}

SignalHandler::Result CollectorOutput::SensorOutput(const sensor::MsgFromCollector& msg) {
  for (auto& client : sensor_clients_) {
    auto res = client->SendMsg(msg);
    switch (res) {
      case SignalHandler::PROCESSED:
        break;

      case SignalHandler::ERROR:
        HandleOutputError();
        return res;

      case SignalHandler::IGNORED:
      case SignalHandler::NEEDS_REFRESH:
      case SignalHandler::FINISHED:
        return res;
    }
  }
  return SignalHandler::PROCESSED;
}

SignalHandler::Result CollectorOutput::SignalOutput(const sensor::SignalStreamMessage& msg) {
  for (auto& client : signal_clients_) {
    auto res = client->PushSignals(msg);
    switch (res) {
      case SignalHandler::PROCESSED:
        break;

      case SignalHandler::ERROR:
        HandleOutputError();
        return res;

      case SignalHandler::IGNORED:
      case SignalHandler::NEEDS_REFRESH:
      case SignalHandler::FINISHED:
        return res;
    }
  }
  return SignalHandler::PROCESSED;
}

SignalHandler::Result CollectorOutput::SendMsg(const MessageType& msg) {
  auto visitor = [this](auto&& m) {
    using T = std::decay_t<decltype(m)>;
    if constexpr (std::is_same_v<T, sensor::MsgFromCollector>) {
      return SensorOutput(m);
    } else if constexpr (std::is_same_v<T, sensor::SignalStreamMessage>) {
      return SignalOutput(m);
    }

    // Unknown type
    return SignalHandler::ERROR;
  };

  return std::visit(visitor, msg);
}

void CollectorOutput::EstablishGrpcStream() {
  while (EstablishGrpcStreamSingle()) {
  }
  CLOG(INFO) << "Service client terminating.";
}

bool CollectorOutput::EstablishGrpcStreamSingle() {
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
  bool success = true;
  for (const auto& client : signal_clients_) {
    success &= client->Refresh();
  }

  for (const auto& client : sensor_clients_) {
    success &= client->Refresh();
  }

  if (success) {
    CLOG(INFO) << "Successfully established GRPC stream.";
    stream_active_.store(true, std::memory_order_release);
  } else {
    CLOG(WARNING) << "Failed to establish GRPC stream, retrying...";
  }
  return true;
}
}  // namespace collector
