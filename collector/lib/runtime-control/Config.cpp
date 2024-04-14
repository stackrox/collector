#include <runtime-control/Config.h>

namespace collector::runtime_control {

Config& Config::GetOrCreate() {
  static Config config;

  return config;
}

bool Config::WaitUntilInitialized(unsigned int timeout_ms) {

}

}