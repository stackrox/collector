#ifndef _RUNTIME_CONTROL_SERVICE_H_
#define _RUNTIME_CONTROL_SERVICE_H_

#include <memory>
#include <thread>

#include <grpcpp/grpcpp.h>

#include <DuplexGRPC.h>
#include <internalapi/sensor/collector_iservice.grpc.pb.h>

namespace collector::runtime_control {

// Creates and manages the configuration channel to Sensor
class Service {
 public:
  Service() = default;
  ~Service();

  void Init(std::shared_ptr<grpc::Channel> control_channel);
  void Start();
  void Stop(bool wait = true);

 private:
  std::shared_ptr<grpc::Channel> control_channel_;

  std::thread thread_;
  std::mutex global_mutex_;
  grpc::ClientContext client_context_;
  std::unique_ptr<IDuplexClientWriter<sensor::MsgFromCollector>> writer_;

  void Receive(const sensor::MsgToCollector* message);

  void Run();
  bool IsRunning();
};

}

#endif