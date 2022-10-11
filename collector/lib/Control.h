#ifndef COLLECTOR_CONTROL_H
#define COLLECTOR_CONTROL_H

namespace collector {

enum ControlValue {
  RUN = 0,           // Keep running
  INTERRUPT_SYSDIG,  // Stop running sysdig, but resume collector operation (e.g., for chisel update)
  STOP_COLLECTOR,    // Stop the collector (e.g., SIGINT or SIGTERM received).
};

}

#endif
