#include <runtime-control/Service.h>

namespace collector::runtime_control {

Service::~Service() {
  Stop();
}

void Service::Init(std::shared_ptr<grpc::Channel> control_channel) {
  this->control_channel_ = control_channel;
}

void Service::Start() {
  std::unique_lock<std::mutex> lock(global_mutex_);

  if (!IsRunning()) {
    thread_ = std::thread(&Service::Run, this);
  }
}

void Service::Stop(bool wait) {
  std::unique_lock<std::mutex> lock(global_mutex_);

  if (IsRunning()) {
    // TODO
    if (wait) {
      thread_.join();
    } else {
      thread_.detach();
    }
  }
}

void Service::Run() {
  writer_ = DuplexClient::CreateWithReadCallback(
    &sensor::CollectorService::Stub::AsyncCommunicate,
    control_channel_,
    &client_context_,
    std::function([this](const sensor::MsgToCollector* message) {
      Receive(message);
    }));

    //while (writer_->Sleep((1s)));
}

bool Service::IsRunning() {
  return thread_.joinable();
}

}