#include "NetworkConnectionInfoServiceComm.h"

#include "Utility.h"
#include "output/GRPCUtil.h"

namespace collector {

constexpr char NetworkConnectionInfoServiceComm::kHostnameMetadataKey[];
constexpr char NetworkConnectionInfoServiceComm::kCapsMetadataKey[];
constexpr char NetworkConnectionInfoServiceComm::kSupportedCaps[];

std::unique_ptr<grpc::ClientContext> NetworkConnectionInfoServiceComm::CreateClientContext() const {
  auto ctx = MakeUnique<grpc::ClientContext>();
  ctx->AddMetadata(kHostnameMetadataKey, hostname_);
  ctx->AddMetadata(kCapsMetadataKey, kSupportedCaps);
  return ctx;
}

NetworkConnectionInfoServiceComm::NetworkConnectionInfoServiceComm(std::string hostname, std::shared_ptr<grpc::Channel> channel) : hostname_(std::move(hostname)), channel_(std::move(channel)) {
  if (channel_) {
    stub_ = sensor::NetworkConnectionInfoService::NewStub(channel_);
  }
}

void NetworkConnectionInfoServiceComm::ResetClientContext() {
  WITH_LOCK(context_mutex_) {
    context_ = CreateClientContext();
  }
}

bool NetworkConnectionInfoServiceComm::WaitForConnectionReady(const std::function<bool()>& check_interrupted) {
  if (!channel_) {
    return true;
  }

  return output::WaitForChannelReady(channel_, check_interrupted);
}

void NetworkConnectionInfoServiceComm::TryCancel() {
  WITH_LOCK(context_mutex_) {
    if (context_) context_->TryCancel();
  }
}

std::unique_ptr<output::IDuplexClientWriter<sensor::NetworkConnectionInfoMessage>> NetworkConnectionInfoServiceComm::PushNetworkConnectionInfoOpenStream(std::function<void(const sensor::NetworkFlowsControlMessage*)> receive_func) {
  if (!context_)
    ResetClientContext();

  if (channel_) {
    return output::DuplexClient::CreateWithReadCallback(
        &sensor::NetworkConnectionInfoService::Stub::AsyncPushNetworkConnectionInfo,
        channel_, context_.get(), std::move(receive_func));
  } else {
    return MakeUnique<output::grpc_duplex_impl::StdoutDuplexClientWriter<sensor::NetworkConnectionInfoMessage>>();
  }
}

}  // namespace collector
