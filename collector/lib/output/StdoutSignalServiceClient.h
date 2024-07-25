#ifndef _COLLECTOR_STDOUT_SIGNAL_SVC_CLIENT
#define _COLLECTOR_STDOUT_SIGNAL_SVC_CLIENT

#include "OutputClient.h"

namespace collector::output {

class StdoutSignalServiceClient : public OutputClient {
 public:
  explicit StdoutSignalServiceClient() {}

  void Start(){};
  void Stop(){};
  bool Ready() { return true; }

  bool PushSignals(const OutputClient::Message& msg);
};

};  // namespace collector::output

#endif
