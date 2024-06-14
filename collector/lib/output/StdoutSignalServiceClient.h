#ifndef _COLLECTOR_STDOUT_SIGNAL_SVC_CLIENT
#define _COLLECTOR_STDOUT_SIGNAL_SVC_CLIENT

#include "ISignalServiceClient.h"

namespace collector::output {

class StdoutSignalServiceClient : public ISignalServiceClient {
 public:
  using SignalStreamMessage = sensor::SignalStreamMessage;

  explicit StdoutSignalServiceClient() {}

  void Start(){};
  void Stop(){};
  bool Ready() { return true; }

  SignalHandler::Result PushSignals(const SignalStreamMessage& msg);
};

};  // namespace collector::output

#endif
