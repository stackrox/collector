#ifndef COLLECTOR_SIGNALHANDLER_H
#define COLLECTOR_SIGNALHANDLER_H

#include <string>
#include <vector>

#include "libsinsp/sinsp.h"

namespace collector {

class SignalHandler {
 public:
  enum Result {
    PROCESSED = 0,
    IGNORED,
    ERROR,
    NEEDS_REFRESH,
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

#endif  //COLLECTOR_SIGNALHANDLER_H
