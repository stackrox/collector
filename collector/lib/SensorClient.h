#ifndef _SENSOR_CLIENT_H_
#define _SENSOR_CLIENT_H_

#include <condition_variable>
#include <memory>

#include <grpcpp/channel.h>

#include "internalapi/sensor/collector_iservice.grpc.pb.h"
#include "internalapi/sensor/collector_iservice.pb.h"

#include "DuplexGRPC.h"
#include "SignalHandler.h"
#include "StoppableThread.h"

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

  virtual bool Refresh() = 0;

  virtual SignalHandler::Result SendMsg(const sensor::MsgFromCollector& msg) = 0;
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

  bool Refresh() override;

  SignalHandler::Result SendMsg(const sensor::MsgFromCollector& msg) override;

 private:
  std::shared_ptr<grpc::Channel> channel_;

  std::atomic<bool> stream_active_ = false;

  // This needs to have the same lifetime as the class.
  std::unique_ptr<grpc::ClientContext> context_;
  std::unique_ptr<IDuplexClientWriter<sensor::MsgFromCollector>> writer_;

  bool first_write_ = false;
};

class SensorClientStdout : public ISensorClient {
  bool Refresh() override { return true; }

  SignalHandler::Result SendMsg(const sensor::MsgFromCollector& msg) override {
    LogProtobufMessage(msg);
    return SignalHandler::PROCESSED;
  }
};

}  // namespace collector

#endif  //_SENSOR_CLIENT_H_
