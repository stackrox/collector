#ifndef OUTPUT_IGRPC_CLIENT
#define OUTPUT_IGRPC_CLIENT

#include <grpcpp/channel.h>

namespace collector::output::grpc {
using GrpcChannel = ::grpc::Channel;
using ClientContext = ::grpc::ClientContext;

class IGrpcClient {
 public:
  IGrpcClient() = default;
  IGrpcClient(const IGrpcClient&) = default;
  IGrpcClient(IGrpcClient&&) = delete;
  IGrpcClient& operator=(const IGrpcClient&) = default;
  IGrpcClient& operator=(IGrpcClient&&) = delete;
  virtual ~IGrpcClient() = default;

  /**
   * Recreate the internal state of the client.
   *
   * @returns true if the refresh was successful, false otherwise.
   */
  virtual bool Recreate() = 0;
};
}  // namespace collector::output::grpc

#endif
