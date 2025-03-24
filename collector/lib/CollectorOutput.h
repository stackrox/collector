#ifndef COLLECTOR_OUTPUT_H
#define COLLECTOR_OUTPUT_H

#include <variant>

#include "internalapi/sensor/collector_iservice.pb.h"
#include "internalapi/sensor/signal_iservice.pb.h"

#include "CollectorConfig.h"
#include "SensorClient.h"
#include "SignalHandler.h"
#include "SignalServiceClient.h"

namespace collector {

using MessageType = std::variant<sensor::MsgFromCollector, sensor::SignalStreamMessage>;

class CollectorOutput {
 public:
  CollectorOutput(const CollectorOutput&) = default;
  CollectorOutput(CollectorOutput&&) = delete;
  CollectorOutput& operator=(const CollectorOutput&) = delete;
  CollectorOutput& operator=(CollectorOutput&&) = delete;

  CollectorOutput(const CollectorConfig& config) {
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

  ~CollectorOutput() {
    StopClients(sensor_clients_);
    StopClients(signal_clients_);
  }

  SignalHandler::Result SendMsg(const MessageType& msg);

 private:
  template <typename T>
  void StartClients(std::vector<T>& clients) {
    for (auto& client : clients) {
      client->Start();
    }
  }

  template <typename T>
  void StopClients(std::vector<T>& clients) {
    for (auto& client : clients) {
      client->Stop();
    }
  }

  std::vector<std::unique_ptr<ISensorClient>> sensor_clients_;
  std::vector<std::unique_ptr<ISignalServiceClient>> signal_clients_;
};

}  // namespace collector

#endif  // COLLECTOR_OUTPUT_H
