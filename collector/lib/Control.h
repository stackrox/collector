#ifndef COLLECTOR_CONTROL_H
#define COLLECTOR_CONTROL_H

namespace collector {

enum ControlValue {
  RUN = 0,         // Keep running
  STOP_COLLECTOR,  // Stop the collector (e.g., SIGINT or SIGTERM received).
};

}

#endif
