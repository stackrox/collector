#ifndef __PROCESS_SIGNAL_HANDLER_H__
#define __PROCESS_SIGNAL_HANDLER_H__

#include <memory>

#include <grpcpp/channel.h>

#include "ProcessSignalFormatter.h"
#include "RateLimit.h"
#include "SignalHandler.h"
#include "system-inspector/Service.h"

// forward declarations
class sinsp;
class sinsp_evt;
class sinsp_threadinfo;

namespace collector {

class ProcessSignalHandler : public SignalHandler {
 public:
  ProcessSignalHandler(sinsp* inspector, output::OutputClient* client, system_inspector::Stats* stats)
      : client_(client), formatter_(inspector), stats_(stats) {}

  bool Start() override;
  bool Stop() override;
  SignalHandlerResult HandleSignal(sinsp_evt* evt) override;
  SignalHandlerResult HandleExistingProcess(sinsp_threadinfo* tinfo) override;
  std::string GetName() override { return "ProcessSignalHandler"; }
  std::vector<std::string> GetRelevantEvents() override;

 private:
  output::OutputClient* client_;
  ProcessSignalFormatter formatter_;
  system_inspector::Stats* stats_;
  RateLimitCache rate_limiter_;
};

}  // namespace collector

#endif  // __PROCESS_SIGNAL_HANDLER_H__
