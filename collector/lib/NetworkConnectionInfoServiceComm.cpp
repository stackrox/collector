#include "NetworkConnectionInfoServiceComm.h"

#include "GRPCUtil.h"
#include "HostInfo.h"
#include "Utility.h"

namespace collector {

constexpr char NetworkConnectionInfoServiceComm::kHostnameMetadataKey[];
constexpr char NetworkConnectionInfoServiceComm::kCapsMetadataKey[];
constexpr char NetworkConnectionInfoServiceComm::kSupportedCaps[];

std::unique_ptr<grpc::ClientContext> NetworkConnectionInfoServiceComm::CreateClientContext() const {
  auto ctx = std::make_unique<grpc::ClientContext>();
  ctx->AddMetadata(kHostnameMetadataKey, HostInfo::GetHostname());
  ctx->AddMetadata(kCapsMetadataKey, kSupportedCaps);
  return ctx;
}

NetworkConnectionInfoServiceComm::NetworkConnectionInfoServiceComm(std::shared_ptr<grpc::Channel> channel) : channel_(std::move(channel)) {
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

  return WaitForChannelReady(channel_, check_interrupted);
}

void NetworkConnectionInfoServiceComm::TryCancel() {
  WITH_LOCK(context_mutex_) {
    if (context_) {
      context_->TryCancel();
    }
  }
}

std::unique_ptr<IDuplexClientWriter<sensor::NetworkConnectionInfoMessage>> NetworkConnectionInfoServiceComm::PushNetworkConnectionInfoOpenStream(std::function<void(const sensor::NetworkFlowsControlMessage*)> receive_func) {
  if (!context_) {
    ResetClientContext();
  }

  if (channel_) {
    return DuplexClient::CreateWithReadCallback(
        &sensor::NetworkConnectionInfoService::Stub::AsyncPushNetworkConnectionInfo,
        channel_, context_.get(), std::move(receive_func));
  } else {
    return std::make_unique<collector::grpc_duplex_impl::StdoutDuplexClientWriter<sensor::NetworkConnectionInfoMessage>>();
  }
}

}  // namespace collector
