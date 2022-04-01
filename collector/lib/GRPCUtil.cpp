#include "GRPCUtil.h"

namespace collector {

bool WaitForChannelReady(const std::shared_ptr<grpc::Channel>& channel,
                         const std::function<bool()>& check_interrupted,
                         const std::chrono::nanoseconds& poll_interval) {
  while (!check_interrupted()) {
    if (channel->WaitForConnected(std::chrono::system_clock::now() + poll_interval)) {
      return true;
    }
    if (channel->GetState(false) == GRPC_CHANNEL_SHUTDOWN) {
      CLOG(FATAL) << "Fatal error waiting for GRPC channel to connect; channel is in shutdown state.";
    }
  }

  return false;
}

}  // namespace collector
