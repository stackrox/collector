
#include "HostInfo.h"

#include <fstream>

#include "Logging.h"

namespace collector {

namespace {

// Helper to construct an absolute path based on mount location
// of various on-host files.
std::string pathOnHost(const char* path) {
  static const char* root = "/host";
  std::stringstream stream;
  stream << root << path;
  return stream.str();
}

// Reads a named value from the os-release file (either in /etc/ or in /usr/lib)
// and filters for a specific name. The file is in the format <NAME>="<VALUE>"
// Quotes are removed from the value, if found. If not found, an empty string is returned.
std::string getOSReleaseValue(const char* name) {
  std::ifstream release_file(pathOnHost("/etc/os-release"));
  if (!release_file.is_open()) {
    release_file.open(pathOnHost("/usr/lib/os-release"));
    if (!release_file.is_open()) {
      return "";
    }
  }

  std::string line;
  while (std::getline(release_file, line)) {
    std::istringstream stream(line);
    std::string key;
    std::string value;

    std::getline(stream, key, '=');
    std::getline(stream, value);

    if (key == name) {
      // ensure we remove quotations from the start and end, if they exist.
      if (*value.begin() == '"') {
        value.erase(0, 1);
      }
      if (*(value.end() - 1) == '"') {
        value.erase(value.size() - 1);
      }
      return value;
    }
  }
  return "";
}

}  // namespace

KernelVersion HostInfo::GetKernelVersion() {
  if (kernel_version_.release.empty()) {
    kernel_version_ = KernelVersion::FromHost();
  }
  return kernel_version_;
}

std::string& HostInfo::GetHostname() {
  if (hostname_.empty()) {
    const char* hostname_env = std::getenv("NODE_HOSTNAME");
    if (hostname_env && *hostname_env) {
      hostname_ = std::string(hostname_env);
    } else {
      CLOG(INFO) << "environment variable NODE_HOSTNAME not set";
      // if we can't get the hostname from the environment
      // we can look in /proc (mounted at /host/proc in the collector container)
      std::ifstream file(pathOnHost("/proc/sys/kernel/hostname"));
      if (!file.is_open()) {
        CLOG(INFO) << "sys/kernel/hostname file not found";
        CLOG(WARNING) << "Failed to determine hostname";
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
