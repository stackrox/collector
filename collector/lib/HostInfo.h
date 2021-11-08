#ifndef _HOSTINFO_H
#define _HOSTINFO_H

extern "C" {
#include <sys/utsname.h>
};

#include <string>

#include "Utility.h"

namespace collector {

const int MIN_RHEL_BUILD_ID = 957;
const int MAX_RHEL_BUILD_ID = 1062;

struct KernelVersion {
  static KernelVersion Get() {
    const char* release = nullptr;
    const char* version = nullptr;
    const char* kernel_version_env = std::getenv("KERNEL_VERSION");
    if (kernel_version_env && *kernel_version_env) {
      release = kernel_version_env;
    }

    struct utsname uts_buffer {};
    if (uname(&uts_buffer) == 0) {
      if (release != nullptr) {
        release = uts_buffer.release;
      }
      version = uts_buffer.version;
    }

    int major = 0;
    int minor = 0;
    int patch = 0;
    int build_id = 0;

    // used to skip individual characters in the stream
    char skip;
    std::stringstream stream(release);

    // expected format:
    // {major}.{minor}.{patch}[-{build_id}]
    // where -{build_id} is optional

    stream >> major >> skip;
    stream >> minor >> skip;
    stream >> patch >> skip;

    // if there is no build id field, this is still safe (though the stream's
    // error bit will be set because it's unlikely to be an integer)
    stream >> build_id;

    return KernelVersion{major, minor, patch, build_id, {release}, {version}};
  }

  bool HasEBPFSupport() {
    if (major < 4 || (major == 4 && minor < 14)) {
      return false;
    }
    return true;
  }

  // the kernel major version
  int major;
  // the kernel minor version
  int minor;
  // the kernel patch version
  int patch;
  // the kernel build id
  int build_id;
  // the entire release string (as in `uname -r`)
  std::string release;
  // the entire version string (as in `uname -v`)
  std::string version;
};

class HostInfo {
  KernelVersion GetKernelVersion() {
    if (kernel_version_.release.empty()) {
      kernel_version_ = KernelVersion::Get();
    }
    return kernel_version_;
  }

  std::string& GetHostname() {
    if (hostname_.empty()) {
      hostname_ = collector::GetHostname();
    }
    return hostname_;
  }

  std::string& GetDistro() {
    if (distro_.empty()) {
      distro_ = collector::GetDistro();
    }
    return distro_;
  }

  std::string& GetBuildID() {
    if (build_id_.empty()) {
      build_id_ = collector::GetBuildID();
    }
    return build_id_;
  }

  std::string& GetOSID() {
    if (os_id_.empty()) {
      os_id_ = collector::GetOSID();
    }
    return os_id_;
  }

  bool IsCOS() {
    return GetOSID() == "cos" && !GetBuildID().empty();
  }

  bool IsCoreOS() {
    return GetOSID() == "coreos";
  }

  bool IsDockerDesktop() {
    return GetDistro() == "Docker Desktop";
  }

  bool IsRHEL76() {
    auto kernel = GetKernelVersion();
    if (GetOSID() == "rhel" || GetOSID() == "centos") {
      // example release version: 3.10.0-957.10.1.el7.x86_64
      // build_id = 957
      if (kernel.release.find(".el7.") != std::string::npos) {
        if (kernel.major == 3 && kernel.minor == 10) {
          return kernel.build_id > MIN_RHEL_BUILD_ID && kernel.build_id < MAX_RHEL_BUILD_ID;
        }
      }
    }
    return false;
  }

  bool HasEBPFSupport() {
    if (IsRHEL76()) {
      return true;
    }
    return GetKernelVersion().HasEBPFSupport();
  }

 private:
  KernelVersion kernel_version_;
  std::string distro_;
  std::string hostname_;
  std::string build_id_;
  std::string os_id_;
};

}  // namespace collector

#endif  // _HOSTINFO_H
