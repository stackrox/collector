#include <GRPCUtil.h>
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
  CLOG(DEBUG) << "[runtime-control::Service] Start";

  while (should_run_) {
    if (WaitForChannelReady(control_channel_, [this]() -> bool { return !should_run_; })) {
      CLOG(DEBUG) << "[runtime-control::Service] Channel is ready";
      reader_ = DuplexClient::CreateWithReadCallback(
          &sensor::CollectorService::Stub::AsyncCommunicate,
          control_channel_,
          &client_context_,
          std::function([this](const sensor::MsgToCollector* message) {
            Receive(message);
          }));

      SessionLoop();

      reader_->Finish();
    }
  }

  CLOG(DEBUG) << "[runtime-control::Service] Shutdown";
}

void Service::SessionLoop() {
  while (should_run_) {
    if (!reader_->Sleep(1s)) {
      CLOG(WARNING) << "[runtime-control::Service] Connection interrupted";
      break;
    }

    // TODO
  }
}

void Service::Receive(const sensor::MsgToCollector* message) {
  if (!message) {
    return;
  }

  switch (message->msg_case()) {
    //case sensor::MsgToCollector::kConfigWithCluster: {
    case sensor::MsgToCollector::kCollectorConfig: {
      sensor::MsgToCollector msg;

      CLOG(INFO) << "[runtime-control::Service] Receive: CollectorRuntimeConfig";
      Config::GetOrCreate().Update(message->config_with_cluster());

      //msg.mutable_collector_runtime_config_ack();
      //writer_->WriteAsync(msg);

      break;
    }
    default:
      CLOG(WARNING) << "[runtime-control::Service] Unhandled object with id=" << message->msg_case();
  }
}

}  // namespace collector::runtime_control
