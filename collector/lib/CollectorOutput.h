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
  CollectorOutput(const CollectorOutput&) = delete;
  CollectorOutput(CollectorOutput&&) = delete;
  CollectorOutput& operator=(const CollectorOutput&) = delete;
  CollectorOutput& operator=(CollectorOutput&&) = delete;

  CollectorOutput(const CollectorConfig& config);

  ~CollectorOutput() {
    stream_interrupted_.notify_one();
    if (thread_.running()) {
      thread_.Stop();
    }
  }

  // Constructor for tests
  CollectorOutput(std::unique_ptr<ISensorClient>&& sensor_client,
                  std::unique_ptr<ISignalServiceClient>&& signal_client) {
    sensor_clients_.emplace_back(std::move(sensor_client));
    signal_clients_.emplace_back(std::move(signal_client));
  }

  SignalHandler::Result SendMsg(const MessageType& msg);
  void Register();

  bool UseSensorClient() const { return use_sensor_client_; }

 private:
  void EstablishGrpcStream();
  bool EstablishGrpcStreamSingle();

  void HandleOutputError();
  SignalHandler::Result SensorOutput(const sensor::MsgFromCollector& msg);
  SignalHandler::Result SignalOutput(const sensor::SignalStreamMessage& msg);

  std::vector<std::unique_ptr<ISensorClient>> sensor_clients_;
  std::vector<std::unique_ptr<ISignalServiceClient>> signal_clients_;

  bool use_sensor_client_{true};

  StoppableThread thread_;
  std::atomic<bool> stream_active_{false};
  std::condition_variable stream_interrupted_;
  std::shared_ptr<grpc::Channel> channel_;
};

}  // namespace collector

#endif  // COLLECTOR_OUTPUT_H
