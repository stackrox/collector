#include "NetworkConnectionClient.h"

#include "internalapi/sensor/network_connection_iservice.grpc.pb.h"

#include "DuplexGRPC.h"
#include "HostInfo.h"

namespace collector::output::grpc {
bool NetworkConnectionClient::Recreate() {
  std::unique_lock<std::mutex> l{mutex_};

  context_ = NewContext();
  if (use_sensor_client_) {
    writer_ = NewWriter();

    if (!writer_->WaitUntilStarted(std::chrono::seconds(30))) {
      CLOG(ERROR) << "Process stream not ready after 30 seconds. Retrying ...";
      CLOG(ERROR) << "Error message: " << writer_->FinishNow().error_message();
      writer_.reset();
      return false;
    }
  } else {
    legacy_writer_ = NewLegacyWriter();

    if (!legacy_writer_->WaitUntilStarted(std::chrono::seconds(30))) {
      CLOG(ERROR) << "Process stream not ready after 30 seconds. Retrying ...";
      CLOG(ERROR) << "Error message: " << writer_->FinishNow().error_message();
      writer_.reset();
      return false;
    }
  }

  return true;
}

SignalHandler::Result NetworkConnectionClient::SendMsg(const sensor::NetworkConnectionInfoMessage& msg) {
  std::unique_lock<std::mutex> l{mutex_};
  grpc_duplex_impl::Result res(grpc_duplex_impl::Status::OK);

  if (use_sensor_client_) {
    res = writer_->Write(msg.info());
  } else {
    res = legacy_writer_->Write(msg);
  }

  if (!res) {
    auto status = writer_->FinishNow();
    if (!status.ok()) {
      CLOG(ERROR) << "GRPC writes failed: (" << status.error_code() << ") " << status.error_message();
    }
    writer_.reset();
    return SignalHandler::ERROR;
  }

  return SignalHandler::PROCESSED;
}

std::unique_ptr<::grpc::ClientContext> NetworkConnectionClient::NewContext() {
  // Before creating a new context, we need to make sure the old one
  // goes unused.
  writer_.reset();
  legacy_writer_.reset();

  auto ctx = std::make_unique<::grpc::ClientContext>();
  ctx->AddMetadata(static_cast<const char*>(kHostnameMetadataKey), HostInfo::GetHostname());
  ctx->AddMetadata(static_cast<const char*>(kCapsMetadataKey), static_cast<const char*>(kSupportedCaps));
  return ctx;
}

std::unique_ptr<IDuplexClientWriter<sensor::NetworkConnectionInfo>> NetworkConnectionClient::NewWriter() {
  return DuplexClient::CreateWithReadCallback(
      &sensor::CollectorService::Stub::AsyncPushNetworkConnectionInfo,
      channel_,
      context_.get(),
      {[this](const auto* msg) {
        // Creates a copy of the message. We are not expecting to receive
        // the control message too often, so it should be fine for now.
        if (msg != nullptr) {
          network_control_channel_ << *msg;
        }
      }});
}

std::unique_ptr<IDuplexClientWriter<sensor::NetworkConnectionInfoMessage>> NetworkConnectionClient::NewLegacyWriter() {
  return DuplexClient::CreateWithReadCallback(
      &sensor::NetworkConnectionInfoService::Stub::AsyncPushNetworkConnectionInfo,
      channel_,
      context_.get(),
      {[this](const auto* msg) {
        // Creates a copy of the message. We are not expecting to receive
        // the control message too often, so it should be fine for now.
        if (msg != nullptr) {
          network_control_channel_ << *msg;
        }
      }});
}
}  // namespace collector::output::grpc
