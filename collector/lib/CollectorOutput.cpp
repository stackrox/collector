#include "CollectorOutput.h"

#include "internalapi/sensor/collector.pb.h"
#include "internalapi/sensor/collector_iservice.pb.h"

#include "HostInfo.h"

namespace collector {

CollectorOutput::CollectorOutput(const CollectorConfig& config) {
  if (config.grpc_channel != nullptr) {
    auto sensor_client = std::make_unique<SensorClient>(config.grpc_channel);
    auto signal_client = std::make_unique<SignalServiceClient>(config.grpc_channel);
    sensor_clients_.emplace_back(std::move(sensor_client));
    signal_clients_.emplace_back(std::move(signal_client));
  }

  if (config.UseStdout()) {
    auto sensor_client = std::make_unique<SensorClientStdout>();
    auto signal_client = std::make_unique<StdoutSignalServiceClient>();
    sensor_clients_.emplace_back(std::move(sensor_client));
    signal_clients_.emplace_back(std::move(signal_client));
  }

  if (sensor_clients_.empty() || signal_clients_.empty()) {
    CLOG(FATAL) << "No available output configured";
  }

  StartClients(sensor_clients_);
  StartClients(signal_clients_);
}

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

void CollectorOutput::Register() {
  sensor::MsgFromCollector msg;
  msg.clear_info();
  msg.clear_process_signal();
  msg.mutable_register_()->set_hostname(GetHostname());

  for (const auto& client : sensor_clients_) {
    auto res = client->SendMsg(msg);
    if (res != SignalHandler::PROCESSED) {
      use_sensor_client_ = false;
      break;
    }
  }
}
}  // namespace collector
