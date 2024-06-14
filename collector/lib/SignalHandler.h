#ifndef COLLECTOR_SIGNALHANDLER_H
#define COLLECTOR_SIGNALHANDLER_H

#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "output/SignalServiceClient.h"

// forward declarations
class sinsp_evt;
class sinsp_threadinfo;

namespace collector {

enum SignalHandlerControl {
  // The event was processed correctly
  PROCESSED = 0,
  // The signal handler can't handle this event
  IGNORED,
  // An error occurred when handling the event
  ERROR,
  // Refresh the process table (handle existing processes)
  NEEDS_REFRESH,
  // Processing has finished, unload this signal handler.
  FINISHED,
};

using SignalHandlerResult = std::tuple<std::optional<output::SignalStreamMessage>, SignalHandlerControl>;

class SignalHandler {
 public:
  virtual std::string GetName() = 0;
  virtual bool Start() { return true; }
  virtual bool Stop() { return true; }
  virtual SignalHandlerResult HandleSignal(sinsp_evt* evt) = 0;
  virtual SignalHandlerResult HandleExistingProcess(sinsp_threadinfo* tinfo) {
    return {std::nullopt, IGNORED};
  }
  virtual std::vector<std::string> GetRelevantEvents() = 0;
};

}  // namespace collector

#endif  // COLLECTOR_SIGNALHANDLER_H
