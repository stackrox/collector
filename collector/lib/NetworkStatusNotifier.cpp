//
// Created by Malte Isberner on 9/18/18.
//

#include "NetworkStatusNotifier.h"

namespace collector {

void NetworkStatusNotifier::Run() {
  while (!thread_.should_stop()) {
    v1::Empty empty;
    auto client_writer = stub_->PushNetworkConnectionInfo(&context_, &empty);

    if (!RunSingle(client_writer.get())) {
      break;
    }
  }
}

void NetworkStatusNotifier::Start() {
  thread_.Start([this]{ Run(); });
}

void NetworkStatusNotifier::Stop() {
  thread_.Stop();
  context_.TryCancel();
}

bool NetworkStatusNotifier::RunSingle(grpc::ClientWriter<sensor::NetworkConnectionInfoMessage>* writer) {
  ConnMap old_state;

  do {
    auto new_state = this->conn_tracker_->FetchState(true);
    ConnectionTracker::ComputeDelta(new_state, &old_state);

    const auto* msg = CreateInfoMessage(old_state);
    old_state = std::move(new_state);

    if (!msg) continue;

    if (!writer->Write(*msg)) {
      break;
    }
  } while (thread_.Pause(std::chrono::seconds(30)));

  auto status = writer->Finish();
  if (!status.ok()) {
    CLOG(ERROR) << "Failed to write network connection info: " << status.error_message();
  }

  return !thread_.should_stop();
}

const sensor::NetworkConnectionInfoMessage* NetworkStatusNotifier::CreateMessage(const ConnMap& delta) {
  if (delta.empty()) {
    return nullptr;
  }

  Reset();
  const auto* msg = AllocateRoot();


}


}  // namespace collector