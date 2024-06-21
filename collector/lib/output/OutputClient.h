#ifndef __SIGNAL_SERVICE_CLIENT_H
#define __SIGNAL_SERVICE_CLIENT_H

// SIGNAL_SERVICE_CLIENT.h
// This class defines our GRPC client abstraction

#include <mutex>

#include <grpc/grpc.h>
#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>

#include "api/v1/signal.pb.h"
#include "internalapi/sensor/signal_iservice.grpc.pb.h"

namespace collector::output {

class OutputClient {
 public:
  using Message = sensor::SignalStreamMessage;
  virtual void Start() = 0;
  virtual void Stop() = 0;
  virtual bool Ready() = 0;
  virtual bool PushSignals(const Message& msg) = 0;

  virtual ~OutputClient() {}
};

}  // namespace collector::output

#endif  // __SIGNAL_SERVICE_CLIENT_H
