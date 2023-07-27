#ifndef __PROCESS_SIGNAL_HANDLER_H__
#define __PROCESS_SIGNAL_HANDLER_H__

#include <memory>

#include "libsinsp/sinsp.h"

#include <grpcpp/channel.h>

#include "ProcessSignalFormatter.h"
#include "RateLimit.h"
#include "SignalHandler.h"
#include "SysdigService.h"

namespace collector {

class ProcessSignalHandler : public SignalHandler {
 public:
  ProcessSignalHandler(sinsp* inspector, ISignalServiceClient* client, SysdigStats* stats)
      : client_(client), formatter_(inspector), stats_(stats) {}

  bool Start() override;
  bool Stop() override;
  Result HandleSignal(sinsp_evt* evt) override;
  Result HandleExistingProcess(sinsp_threadinfo* tinfo) override;
  std::string GetName() override { return "ProcessSignalHandler"; }
  std::vector<std::string> GetRelevantEvents() override;

 private:
  ISignalServiceClient* client_;
  ProcessSignalFormatter formatter_;
  SysdigStats* stats_;
  RateLimitCache rate_limiter_;
};

}  // namespace collector

#endif  // __PROCESS_SIGNAL_HANDLER_H__
