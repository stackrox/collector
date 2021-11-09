
#include "HostInfo.h"

#include <fstream>

#include "Logging.h"

namespace collector {

// Reads a named value from the os-release file (either in /etc/ or in /usr/lib)
// and filters for a specific name. The file is in the format <NAME>="<VALUE>"
// Quotes are removed from the value, if found. If not found, an empty string is returned.
static std::string getOSReleaseValue(const char *name) {
  std::ifstream release_file("/host/etc/os-release");
  if (!release_file.is_open()) {
    release_file.open("/host/usr/lib/os-release");
  }

  if (release_file.is_open()) {
    std::string line;
    while (std::getline(release_file, line)) {
      std::istringstream stream(line);
      std::string key;
      std::string value;

      std::getline(stream, key, '=');
      std::getline(stream, value);

      if (key == name) {
        // remove quotes from around the value.
        value.erase(std::remove(value.begin(), value.end(), '"'), value.end());
        return value;
      }
    }
  }

  return "";
}

KernelVersion HostInfo::GetKernelVersion() {
  if (kernel_version_.release.empty()) {
    kernel_version_ = KernelVersion::Get();
  }
  return kernel_version_;
}

std::string& HostInfo::GetHostname() {
  if (hostname_.empty()) {
    const char* hostname_env = std::getenv("NODE_HOSTNAME");
    if (hostname_env && *hostname_env) {
      hostname_ = std::string(hostname_env);
    } else {
      // if we can't get the hostname from the environment
      // we can look in /proc (mounted at /host/proc in the collector container)
      std::ifstream file("/host/proc/sys/kernel/hostname");
      if (!file.is_open()) {
        CLOG(ERROR) << "Failed to determine hostname, environment variable NODE_HOSTNAME not set";
        hostname_ = "unknown";
      } else {
        std::getline(file, hostname_);
      }
    }
  }
  return hostname_;
}

std::string& HostInfo::GetDistro() {
  if (distro_.empty()) {
    std::string pretty_name = getOSReleaseValue("PRETTY_NAME");
    if (pretty_name.empty()) {
      distro_ = "Linux";
    } else {
      distro_ = pretty_name;
    }
  }
  return distro_;
}

std::string& HostInfo::GetBuildID() {
  if (build_id_.empty()) {
    build_id_ = getOSReleaseValue("BUILD_ID");
  }
  return build_id_;
}

std::string& HostInfo::GetOSID() {
  if (os_id_.empty()) {
    os_id_ = getOSReleaseValue("ID");
  }
  return os_id_;
}

}  // namespace collector