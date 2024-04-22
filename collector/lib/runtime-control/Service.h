#ifndef _RUNTIME_CONTROL_SERVICE_H_
#define _RUNTIME_CONTROL_SERVICE_H_

#include <DuplexGRPC.h>
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

#include <grpcpp/grpcpp.h>

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
  std::atomic_bool should_run_ = true;
  grpc::ClientContext client_context_;
  std::unique_ptr<IDuplexClientWriter<sensor::MsgFromCollector>> writer_;

  void Receive(const sensor::MsgToCollector* message);

  void Run();
  void SessionLoop();
};

}  // namespace collector::runtime_control

#endif
