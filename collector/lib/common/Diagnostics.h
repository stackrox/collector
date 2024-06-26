#ifndef COLLECTOR_DIAGNOSTICS_H
#define COLLECTOR_DIAGNOSTICS_H

#include <sstream>
#include <string>

#include "Logging.h"

namespace collector {

class IDiagnostics {
 public:
  virtual void Log() = 0;
};

class StartupDiagnostics : public IDiagnostics {
 public:
  void Log() {
    CLOG(INFO) << "";
    CLOG(INFO) << "== Collector Startup Diagnostics: ==";
    CLOG(INFO) << " Connected to Sensor?       " << std::boolalpha << connectedToSensor_;

    CLOG(INFO) << " Kernel driver candidates:";
    for (const auto& line : driverDiagnostics_) {
      CLOG(INFO) << line;
    }

    CLOG(INFO) << "====================================";
    CLOG(INFO) << "";

    driverDiagnostics_.clear();
  }

  static StartupDiagnostics& GetInstance() {
    static StartupDiagnostics instance;

    return instance;
  }

  void ConnectedToSensor() { connectedToSensor_ = true; }

  void DriverUnavailable(const std::string& candidate) {
    std::stringstream line;
    line << "   " << candidate << " (unavailable)";
    driverDiagnostics_.push_back(line.str());
  }
  void DriverAvailable(const std::string& candidate) {
    std::stringstream line;
    line << "   " << candidate << " (available)";
    driverDiagnostics_.push_back(line.str());
  }

  void DriverSuccess(const std::string& candidate) {
    std::stringstream line;
    line << " Driver loaded into kernel: " << candidate;
    driverDiagnostics_.push_back(line.str());
  }

 private:
  bool connectedToSensor_;
  std::vector<std::string> driverDiagnostics_;

  StartupDiagnostics()
      : connectedToSensor_(false),
        driverDiagnostics_() {}
};

}  // namespace collector

#endif
