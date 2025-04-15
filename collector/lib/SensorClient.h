#pragma once

#include <memory>

#include <grpcpp/channel.h>

#include "internalapi/sensor/collector_iservice.grpc.pb.h"

#include "DuplexGRPC.h"
#include "SignalHandler.h"

namespace collector {

class ISensorClient {
 public:
  using Service = sensor::CollectorService;

  ISensorClient() = default;
  ISensorClient(const ISensorClient&) = default;
  ISensorClient(ISensorClient&&) = delete;
  ISensorClient& operator=(const ISensorClient&) = default;
  ISensorClient& operator=(ISensorClient&&) = delete;
  virtual ~ISensorClient() = default;

  /**
   * Recreate the internal state of the object to allow communication.
   *
   * Mostly useful for handling gRPC reconnections.
   *
   * @returns true if the refresh was succesful, false otherwise.
   */
  virtual bool Recreate() = 0;

  /**
   * Send a message to sensor through the iservice.
   *
   * @param msg The message to be sent to sensor.
   * @returns A SignalHandler::Result with the outcome of the send
   *          operation.
   */
  virtual SignalHandler::Result SendMsg(const sensor::ProcessSignal& msg) = 0;
};

class SensorClient : public ISensorClient {
 public:
  using Service = sensor::CollectorService;

  SensorClient(const SensorClient&) = delete;
  SensorClient(SensorClient&&) = delete;
  SensorClient& operator=(const SensorClient&) = delete;
  SensorClient& operator=(SensorClient&&) = delete;
  ~SensorClient() override {
    context_->TryCancel();
  }

  explicit SensorClient(std::shared_ptr<grpc::Channel> channel)
      : channel_(std::move(channel)) {
  }

  bool Recreate() override;

  SignalHandler::Result SendMsg(const sensor::ProcessSignal& msg) override;

 private:
  std::shared_ptr<grpc::Channel> channel_;

  std::atomic<bool> stream_active_ = false;

  // This needs to have the same lifetime as the class.
  std::unique_ptr<grpc::ClientContext> context_;
  std::unique_ptr<IDuplexClientWriter<sensor::ProcessSignal>> writer_;

  bool first_write_ = false;
};

class SensorClientStdout : public ISensorClient {
  bool Recreate() override { return true; }

  SignalHandler::Result SendMsg(const sensor::ProcessSignal& msg) override {
    LogProtobufMessage(msg);
    return SignalHandler::PROCESSED;
  }
};

}  // namespace collector
