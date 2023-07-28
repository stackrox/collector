#ifndef COLLECTOR_SIGNALHANDLER_H
#define COLLECTOR_SIGNALHANDLER_H

#include <string>
#include <vector>

#include "libsinsp/sinsp.h"

namespace collector {

class SignalHandler {
 public:
  enum Result {
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

  virtual std::string GetName() = 0;
  virtual bool Start() { return true; }
  virtual bool Stop() { return true; }
  virtual Result HandleSignal(sinsp_evt* evt) = 0;
  virtual Result HandleExistingProcess(sinsp_threadinfo* tinfo) {
    return IGNORED;
  }
  virtual std::vector<std::string> GetRelevantEvents() = 0;
};

}  // namespace collector

#endif  // COLLECTOR_SIGNALHANDLER_H
