#ifndef COLLECTOR_DIAGNOSTICS_H
#define COLLECTOR_DIAGNOSTICS_H

#include <sstream>
#include <string>

namespace collector {

class IDiagnostics {
 public:
  virtual std::string Dump() = 0;
};

class StartupDiagnostics : public IDiagnostics {
 public:
  StartupDiagnostics()
      : connectedToSensor_(false),
        kernelDriverDownloaded_(false),
        kernelDriverLoaded_(false) {}

  std::string Dump() {
    std::stringstream ss;
    ss << std::endl;
    ss << "== Collector Startup Diagnostics: ==" << std::endl;
    ss << " Connected to Sensor?       " << std::boolalpha << connectedToSensor_ << std::endl;
    ss << " Kernel driver available?   " << std::boolalpha << kernelDriverDownloaded_ << std::endl;
    ss << " Driver loaded into kernel? " << std::boolalpha << kernelDriverLoaded_ << std::endl;
    ss << "====================================";
    return ss.str();
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
