#ifndef _GRPC_UTIL_H_
#define _GRPC_UTIL_H_

#include <chrono>
#include <functional>

#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
#include <grpcpp/channel.h>
#include <grpcpp/grpcpp.h>

#include "Logging.h"

namespace collector {

bool WaitForChannelReady(
    const std::shared_ptr<grpc::Channel>& channel,
    const std::function<bool()>& check_interrupted = []() { return false; },
    const std::chrono::nanoseconds& poll_interval = std::chrono::seconds(1));

}  // namespace collector

#endif  // _GRPC_UTIL_H_
