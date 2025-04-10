#pragma once

#include <variant>

#include "internalapi/sensor/signal_iservice.pb.h"

#include "CollectorConfig.h"
#include "SensorClient.h"
#include "SignalHandler.h"
#include "SignalServiceClient.h"
#include "StoppableThread.h"

namespace collector::output {

using MessageType = std::variant<sensor::ProcessSignal, sensor::SignalStreamMessage>;

class Output {
 public:
  Output(const Output&) = delete;
  Output(Output&&) = delete;
  Output& operator=(const Output&) = delete;
  Output& operator=(Output&&) = delete;

  Output(const CollectorConfig& config);

  ~Output() {
    stream_interrupted_.notify_one();
    if (thread_.running()) {
      thread_.Stop();
    }
  }

  // Constructor for tests
  Output(std::unique_ptr<ISensorClient>&& sensor_client,
         std::unique_ptr<ISignalServiceClient>&& signal_client) {
    sensor_clients_.emplace_back(std::move(sensor_client));
    signal_clients_.emplace_back(std::move(signal_client));
  }

  /**
   * Send a message to sensor.
   *
   * @param msg One of sensor::MsgFromCollector or
   *            sensor::SignalStreamMessage, the proper service to be
   *            used will be determined from the type held in msg.
   * @returns A SignalHandler::Result with the outcome of the send
   *          operation
   */
  SignalHandler::Result SendMsg(const MessageType& msg);

  /**
   * Whether we should use the new iservice or not.
   *
   * @returns true if configuration indicates we should use the new
   *          iservice, false otherwise.
   */
  bool UseSensorClient() const { return use_sensor_client_; }

 private:
  void EstablishGrpcStream();
  bool EstablishGrpcStreamSingle();

  void HandleOutputError();
  SignalHandler::Result SensorOutput(const sensor::ProcessSignal& msg);
  SignalHandler::Result SignalOutput(const sensor::SignalStreamMessage& msg);

  std::vector<std::unique_ptr<ISensorClient>> sensor_clients_;
  std::vector<std::unique_ptr<ISignalServiceClient>> signal_clients_;

  bool use_sensor_client_ = true;

  StoppableThread thread_;
  std::atomic<bool> stream_active_ = false;
  std::condition_variable stream_interrupted_;
  std::shared_ptr<grpc::Channel> channel_;
};

}  // namespace collector::output
