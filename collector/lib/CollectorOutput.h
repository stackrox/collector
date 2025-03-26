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

  CollectorOutput(const CollectorConfig& config);

  ~CollectorOutput() {
    StopClients(sensor_clients_);
    StopClients(signal_clients_);
  }

  SignalHandler::Result SendMsg(const MessageType& msg);
  void Register();

  bool UseSensorClient() { return use_sensor_client_; }

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

  bool use_sensor_client_{true};
};

}  // namespace collector

#endif  // COLLECTOR_OUTPUT_H
