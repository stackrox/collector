#include "CollectorOutput.h"

namespace collector {

namespace {
SignalHandler::Result SensorOutput(std::vector<std::unique_ptr<ISensorClient>>& clients, const sensor::MsgFromCollector& msg) {
  for (auto& client : clients) {
    auto res = client->SendMsg(msg);
    if (res != SignalHandler::PROCESSED) {
      return res;
    }
  }
  return SignalHandler::PROCESSED;
}

SignalHandler::Result SignalOutput(std::vector<std::unique_ptr<ISignalServiceClient>>& clients, const sensor::SignalStreamMessage& msg) {
  for (auto& client : clients) {
    auto res = client->PushSignals(msg);
    if (res != SignalHandler::PROCESSED) {
      return res;
    }
  }
  return SignalHandler::PROCESSED;
}
}  // namespace

SignalHandler::Result CollectorOutput::SendMsg(const MessageType& msg) {
  auto visitor = [this](auto&& m) {
    using T = std::decay_t<decltype(m)>;
    if constexpr (std::is_same_v<T, sensor::MsgFromCollector>) {
      return SensorOutput(sensor_clients_, m);
    } else if constexpr (std::is_same_v<T, sensor::SignalStreamMessage>) {
      return SignalOutput(signal_clients_, m);
    }

    // Unknown type
    return SignalHandler::ERROR;
  };

  return std::visit(visitor, msg);
}
}  // namespace collector
