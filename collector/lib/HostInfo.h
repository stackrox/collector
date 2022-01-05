/** collector

A full notice with attributions is provided along with this source code.

    This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License version 2 as published by the Free Software Foundation.

                                 This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program; if not, write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

* In addition, as a special exception, the copyright holders give
* permission to link the code of portions of this program with the
* OpenSSL library under certain conditions as described in each
* individual source file, and distribute linked combinations
* including the two.
* You must obey the GNU General Public License in all respects
* for all of the code used other than OpenSSL.  If you modify
* file(s) with this exception, you may extend this exception to your
* version of the file(s), but you are not obligated to do so.  If you
* do not wish to do so, delete this exception statement from your
* version.
*/

#ifndef _HOSTINFO_H
#define _HOSTINFO_H

extern "C" {
#include <sys/utsname.h>
}

#include <regex>
#include <string>

#include "Logging.h"
#include "Utility.h"

namespace collector {

const int MIN_RHEL_BUILD_ID = 957;

struct KernelVersion {
  KernelVersion() : kernel(0), major(0), minor(0), build_id(0) {}

  KernelVersion(std::string release, std::string version)
      : kernel(0), major(0), minor(0), build_id(0), release(std::move(release)), version(std::move(version)) {
    // regex for parsing first parts of release version:
    // ^                   -> must match start of the string
    // (\d+)\.(\d+)\.(\d+) -> match and capture kernel, major, minor versions
    // (-(\d+))?           -> optionally match hyphen followed by build id number
    // .*                  -> matches the rest of the string
    std::regex release_re(R"(^(\d+)\.(\d+)\.(\d+)(-(\d+))?.*)");
    std::smatch match;
    if (!std::regex_match(this->release, match, release_re)) {
      CLOG(ERROR) << "'" << this->release << "' does not match expected kernel version format.";
      return;
    }

    // index zero is the full release string rather than the capture groups
    kernel = std::stoi(match.str(1));
    major = std::stoi(match.str(2));
    minor = std::stoi(match.str(3));

    // not 4, because that's the capture group for the entire '-<build_id>'
    if (!match.str(5).empty()) {
      build_id = std::stoi(match.str(5));
    }
  }

  // Constructs a KernelVersion from host information.
  // First checking the KERNEL_VERSION environment variable, otherwise uses
  // the uname syscall.
  static KernelVersion FromHost() {
    std::string release;
    std::string version;

    const char* kernel_version_env = std::getenv("KERNEL_VERSION");
    if (kernel_version_env && *kernel_version_env) {
      release = kernel_version_env;
    }

    struct utsname uts_buffer {};
    if (uname(&uts_buffer) == 0) {
      if (release.empty()) {
        release = uts_buffer.release;
      }
      version = uts_buffer.version;
      CLOG(DEBUG) << "identified kernel release: '" << release << "'";
      CLOG(DEBUG) << "identified kernel version: '" << version << "'";
    } else {
      CLOG(WARNING) << "uname failed (" << StrError() << ") unable to resolve kernel information";
    }

    return {release, version};
  }

  // Whether or not the kernel has built-in eBPF support
  // Anything before 4.14 doesn't have support, anything newer does.
  bool HasEBPFSupport() const {
    if (kernel < 4 || (kernel == 4 && major < 14)) {
      return false;
    }
    return true;
  }

  // Provides a simple version of the release string
  // containing only the kernel, major, and minor versions.
  std::string ShortRelease() {
    std::stringstream ss;
    ss << kernel << "."
       << major << "."
       << minor;
    return ss.str();
  }

  // the kernel version
  int kernel;
  // the kernel major version
  int major;
  // the kernel minor version
  int minor;
  // the kernel build id
  int build_id;
  // the entire release string (as in `uname -r`)
  std::string release;
  // the entire version string (as in `uname -v`)
  std::string version;
};

// Singleton that provides ways of retrieving Host information to inform
// runtime configuration of collector.
class HostInfo {
 public:
  // Singleton - we're not expecting this Host information to change
  // during execution of collector, so this will be our one source of 'truth'
  // but primarily used during collector startup.
  static HostInfo& Instance() {
    static HostInfo instance;
    return instance;
  }

  // delete the following to ensure singleton pattern
  HostInfo(HostInfo const&) = delete;
  void operator=(HostInfo const&) = delete;

  // Get the Kernel version information for the host.
  virtual KernelVersion GetKernelVersion();

  // Get the host's hostname
  std::string& GetHostname();

  // Get the Linux distribution, if possible.
  // If not, default to "Linux"
  std::string& GetDistro();

  // Get the Build ID of the host.
  virtual std::string& GetBuildID();

  // Get the OS ID of the host
  virtual std::string& GetOSID();

  // Whether we're running on a COS host
  virtual bool IsCOS() {
    return GetOSID() == "cos" && !GetBuildID().empty();
  }

  // Whether we're running on a CoreOS host
  virtual bool IsCoreOS() {
    return GetOSID() == "coreos";
  }

  // Whether we're running on Docker Desktop
  virtual bool IsDockerDesktop() {
    return GetDistro() == "Docker Desktop";
  }

  // Whether we're running on Ubuntu
  virtual bool IsUbuntu() {
    return GetOSID() == "ubuntu";
  }

  // Whether we're running on Garden Linux
  virtual bool IsGarden() {
    return std::regex_match(GetDistro(), std::regex(R"(^Garden Linux \d+\.\d+)"));
  }

  // Reads a named value from the os-release file (either in /etc/ or in /usr/lib)
  // and filters for a specific name. The file is in the format <NAME>="<VALUE>"
  // Quotes are removed from the value, if found. If not found, an empty string is returned.
  virtual std::string GetOSReleaseValue(const char* key);

  // Whether we're running on RHEL 7.6
  // This assumes that RHEL 7.6 will remain on kernel 3.10 and constrains
  // this check to build IDs between MIN_RHEL_BUILD_ID and MAX_RHEL_BUILD_ID
  bool IsRHEL76();

  // Whether this host has eBPF support, based on the kernel version.
  // Only exception is RHEL 7.6, which does support eBPF but runs kernel 3.10 (which ordinarily does
  // not support eBPF)
  bool HasEBPFSupport();

 protected:
  // basic default constructor, doesn't need to do anything,
  // since we're lazy-initializing internal state.
  HostInfo() = default;

 private:
  // the kernel version of the host
  KernelVersion kernel_version_;
  // the Linux distribution of the host (defaults to Linux)
  std::string distro_;
  // the hostname of the host
  std::string hostname_;
  // the build ID of the host (from the release string, and os-release
  std::string build_id_;
  // the OS ID (from os-release file)
  std::string os_id_;
};

}  // namespace collector

#endif  // _HOSTINFO_H
