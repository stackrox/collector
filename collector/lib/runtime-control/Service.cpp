#include <Logging.h>

#include <runtime-control/Config.h>
#include <runtime-control/Service.h>

using namespace std::chrono_literals;

namespace collector::runtime_control {

Service::~Service() {
  Stop();
}

void Service::Init(std::shared_ptr<grpc::Channel> control_channel) {
  this->control_channel_ = control_channel;
}

void Service::Start() {
  std::unique_lock<std::mutex> lock(global_mutex_);

  if (!thread_.joinable()) {
    thread_ = std::thread(&Service::Run, this);
  }
}

void Service::Stop(bool wait) {
  std::unique_lock<std::mutex> lock(global_mutex_);

  should_run_ = false;

  if (thread_.joinable()) {
    lock.unlock();
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

  while (writer_->Sleep(1s)) {
    std::unique_lock<std::mutex> lock(global_mutex_);

    // TODO

    if (!should_run_)
      break;
  }

  writer_->Shutdown();
}

void Service::Receive(const sensor::MsgToCollector* message) {
  if (!message) {
    return;
  }

  switch (message->msg_case()) {
    case sensor::MsgToCollector::kRuntimeFilteringConfiguration: {
      sensor::MsgFromCollector msg;

      CLOG(INFO) << "Receive: RuntimeFilteringConfiguration";
      Config::GetOrCreate().Update(message->runtime_filtering_configuration());

      msg.mutable_runtime_filters_ack();
      writer_->WriteAsync(msg);
    }
    default:
      CLOG(WARNING) << "runtime-control::Service::Receive() unhandled object with id=" << message->msg_case();
  }
}

}  // namespace collector::runtime_control