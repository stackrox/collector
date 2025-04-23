#pragma once

#include <memory>

#include <grpcpp/channel.h>

#include "CollectorConfig.h"
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
  ProcessSignalHandler(
      sinsp* inspector,
      ISignalServiceClient* client,
      system_inspector::Stats* stats,
      const CollectorConfig& config)
      : client_(client),
        formatter_(inspector, config),
        stats_(stats),
        config_(config) {}

  ProcessSignalHandler(const ProcessSignalHandler&) = delete;
  ProcessSignalHandler(ProcessSignalHandler&&) = delete;
  ProcessSignalHandler& operator=(const ProcessSignalHandler&) = delete;
  ProcessSignalHandler& operator=(ProcessSignalHandler&&) = delete;
  ~ProcessSignalHandler() override = default;

  bool Start() override;
  bool Stop() override;
  Result HandleSignal(sinsp_evt* evt) override;
  Result HandleExistingProcess(sinsp_threadinfo* tinfo) override;
  std::string GetName() override { return "ProcessSignalHandler"; }
  std::vector<std::string> GetRelevantEvents() override;

 private:
  ISignalServiceClient* client_;
  ProcessSignalFormatter formatter_;
  system_inspector::Stats* stats_;
  RateLimitCache rate_limiter_;

  const CollectorConfig& config_;
};

}  // namespace collector
