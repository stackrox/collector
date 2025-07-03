#pragma once

#include <grpcpp/channel.h>

#include "CollectorConfig.h"
#include "ProcessSignalFormatter.h"
#include "RateLimit.h"
#include "SensorClientFormatter.h"
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
      CollectorOutput* client,
      system_inspector::Stats* stats,
      const CollectorConfig& config)
      : client_(client),
        signal_formatter_(inspector, config),
        sensor_formatter_(inspector, config),
        stats_(stats) {}

  ProcessSignalHandler(const ProcessSignalHandler&) = delete;
  ProcessSignalHandler(ProcessSignalHandler&&) = delete;
  ProcessSignalHandler& operator=(const ProcessSignalHandler&) = delete;
  ProcessSignalHandler& operator=(ProcessSignalHandler&&) = delete;
  ~ProcessSignalHandler() override = default;

  Result HandleSignal(sinsp_evt* evt) override;
  Result HandleExistingProcess(sinsp_threadinfo* tinfo) override;
  std::string GetName() override { return "ProcessSignalHandler"; }
  std::vector<std::string> GetRelevantEvents() override;

 private:
  // Handlers for the old service
  Result HandleProcessSignal(sinsp_evt* evt);
  Result HandleExistingProcessSignal(sinsp_threadinfo* tinfo);

  // Handlers for the new service
  Result HandleSensorSignal(sinsp_evt* evt);
  Result HandleExistingProcessSensor(sinsp_threadinfo* tinfo);

  CollectorOutput* client_;
  ProcessSignalFormatter signal_formatter_;
  SensorClientFormatter sensor_formatter_;
  system_inspector::Stats* stats_;
  RateLimitCache rate_limiter_;
};

}  // namespace collector
