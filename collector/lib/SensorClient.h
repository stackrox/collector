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

  virtual void Start() = 0;
  virtual void Stop() = 0;

  virtual SignalHandler::Result SendMsg(const sensor::MsgFromCollector& msg) = 0;
};

class SensorClient : public ISensorClient {
 public:
  using Service = sensor::CollectorService;

  explicit SensorClient(std::shared_ptr<grpc::Channel> channel)
      : channel_(std::move(channel)) {
  }

  void Start() override;
  void Stop() override;

  SignalHandler::Result SendMsg(const sensor::MsgFromCollector& msg) override;

 private:
  void EstablishGRPCStream();
  bool EstablishGRPCStreamSingle();

  std::shared_ptr<grpc::Channel> channel_;

  StoppableThread thread_;
  std::atomic<bool> stream_active_ = false;
  std::condition_variable stream_interrupted_;

  // This needs to have the same lifetime as the class.
  std::unique_ptr<grpc::ClientContext> context_;
  std::unique_ptr<IDuplexClientWriter<sensor::MsgFromCollector>> writer_;

  bool first_write_ = false;
};

class SensorClientStdout : public ISensorClient {
  void Start() override {}
  void Stop() override {}

  SignalHandler::Result SendMsg(const sensor::MsgFromCollector& msg) override {
    LogProtobufMessage(msg);
    return SignalHandler::PROCESSED;
  }
};

}  // namespace collector

#endif  //_SENSOR_CLIENT_H_
