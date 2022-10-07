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
  StartupDiagnostics()
      : connectedToSensor_(false),
        kernelDriverDownloaded_(false),
        kernelDriverLoaded_(false) {}

  void Log() {
    CLOG(INFO) << "";
    CLOG(INFO) << "== Collector Startup Diagnostics: ==";
    CLOG(INFO) << " Connected to Sensor?       " << std::boolalpha << connectedToSensor_;
    CLOG(INFO) << " Kernel driver available?   " << std::boolalpha << kernelDriverDownloaded_;
    CLOG(INFO) << " Driver loaded into kernel? " << std::boolalpha << kernelDriverLoaded_;
    CLOG(INFO) << "====================================";
    CLOG(INFO) << "";
  }

  void ConnectedToSensor() { connectedToSensor_ = true; }
  void KernelDriverDownloaded() { kernelDriverDownloaded_ = true; }
  void KernelDriverLoaded() { kernelDriverLoaded_ = true; }

 private:
  bool connectedToSensor_;
  bool kernelDriverDownloaded_;
  bool kernelDriverLoaded_;
};

}  // namespace collector

#endif
