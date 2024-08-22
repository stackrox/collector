#ifndef _RUNTIME_CONTROL_SERVICE_H_
#define _RUNTIME_CONTROL_SERVICE_H_

#include <DuplexGRPC.h>
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

#include <grpcpp/grpcpp.h>

#include <grpcpp/client_context.h> // For grpc::ClientContext
#include <grpcpp/create_channel.h> // For grpc::CreateChannel
#include <grpcpp/support/client_callback.h> // For grpc::ClientReader

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
  std::unique_ptr<IDuplexClientWriter<sensor::MsgToCollector>> writer_;
  std::unique_ptr<IDuplexClient<sensor::MsgToCollector>> reader_;

  void Receive(const sensor::MsgToCollector* message);

  void Run();
  void SessionLoop();
};

}  // namespace collector::runtime_control

#endif
