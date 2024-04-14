#ifndef _RUNTIME_CONTROL_CONFIG_H_
#define _RUNTIME_CONTROL_CONFIG_H_

#include <chrono>

namespace collector::runtime_control {

class Config {
 public:
  static Config& GetOrCreate();

  // returns true when initialized, false for a timeout.
  bool WaitUntilInitialized(unsigned int timeout_ms);
};
}

#endif