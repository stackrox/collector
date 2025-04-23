#include "Output.h"

#include "output/grpc/Client.h"
#include "output/log/Client.h"

namespace collector::output {

Output::Output(const CollectorConfig& config)
    : use_sensor_client_(!config.UseLegacyServices()) {
  if (config.grpc_channel != nullptr) {
    auto sensor_client = std::make_unique<grpc::Client>(config.grpc_channel, client_ready_, use_sensor_client_);
    network_control_channel_ = &sensor_client->GetControlMessageChannel();
    clients_.emplace_back(std::move(sensor_client));
  }

  if (config.grpc_channel == nullptr || config.UseStdout()) {
    auto sensor_client = std::make_unique<log::Client>();
    clients_.emplace_back(std::move(sensor_client));
  }
}

SignalHandler::Result Output::SendMsg(const MsgToSensor& msg) {
  for (auto& client : clients_) {
    auto res = client->SendMsg(msg);
    if (res != SignalHandler::PROCESSED) {
      return res;
    }
  }

  return SignalHandler::PROCESSED;
}

bool Output::IsReady() {
  return std::all_of(clients_.begin(), clients_.end(), [](const auto& client) {
    return client->IsReady();
  });
}

bool Output::WaitReady(const std::function<bool()>& predicate) {
  std::unique_lock<std::mutex> lock{wait_mutex_};
  client_ready_.wait(lock, [this, predicate] {
    return IsReady() || predicate();
  });

  return IsReady();
}
}  // namespace collector::output
