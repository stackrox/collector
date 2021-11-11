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

#include "Utility.h"

namespace collector {

const int MIN_RHEL_BUILD_ID = 957;
const int MAX_RHEL_BUILD_ID = 1062;

struct KernelVersion {
  KernelVersion() : major(0), minor(0), patch(0), build_id(0) {}

  KernelVersion(const char* release, const char* version) : major(0), minor(0), patch(0), build_id(0) {
    this->version = version;
    this->release = release;

    // regex for parsing first parts of release version:
    // ^                   -> must match start of the string
    // (\d+)\.(\d+)\.(\d+) -> match and capture major, minor, patch versions
    // (-(\d+))?           -> optionally match hyphen followed by build id number
    // .*                  -> matches the rest of the string
    std::regex release_re(R"(^(\d+)\.(\d+)\.(\d+)(-(\d+))?.*)");
    std::smatch match;
    if (!std::regex_match(this->release, match, release_re)) {
      return;
    }

    // index zero is the full release string rather than the capture groups
    major = std::stoi(match.str(1));
    minor = std::stoi(match.str(2));
    patch = std::stoi(match.str(3));

    // not 4, because that's the capture group for the entire '-<build_id>'
    if (!match.str(5).empty()) {
      build_id = std::stoi(match.str(5));
    }
  }

  // Constructs a KernelVersion from host information.
  // First checking the KERNEL_VERSION environment variable, otherwise uses
  // the uname syscall.
  static KernelVersion FromHost() {
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

    return {release, version};
  }

  // Whether or not the kernel has built-in eBPF support
  // Anything before 4.14 doesn't have support, anything newer does.
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

// Helper method which checks whether the given kernel & os
// are RHEL 7.6 (to inform later heuristics around eBPF support)
bool IsRHEL76(KernelVersion& kernel, std::string& os_id);

// Helper method which checks whether the given kernel & os
// support eBPF. In practice this is RHEL 7.6, and any kernel
// newer than 4.14
bool HasEBPFSupport(KernelVersion& kernel, std::string& os_id);

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
  KernelVersion GetKernelVersion();

  // Get the host's hostname
  std::string& GetHostname();

  // Get the Linux distribution, if possible.
  // If not, default to "Linux"
  std::string& GetDistro();

  // Get the Build ID of the host.
  std::string& GetBuildID();

  // Get the OS ID of the host
  std::string& GetOSID();

  // Whether we're running on a COS host
  bool IsCOS() {
    return GetOSID() == "cos" && !GetBuildID().empty();
  }

  // Whether we're running on a CoreOS host
  bool IsCoreOS() {
    return GetOSID() == "coreos";
  }

  // Whether we're running on Docker Desktop
  bool IsDockerDesktop() {
    return GetDistro() == "Docker Desktop";
  }

  // Whether we're running on RHEL 7.6
  // This assumes that RHEL 7.6 will remain on kernel 3.10 and constrains
  // this check to build IDs between MIN_RHEL_BUILD_ID and MAX_RHEL_BUILD_ID
  bool IsRHEL76() {
    auto kernel = GetKernelVersion();
    return collector::IsRHEL76(kernel, GetOSID());
  }

  // Whether this host has eBPF support, based on the kernel version.
  // Only exception is RHEL 7.6, which does support eBPF but runs kernel 3.10 (which ordinarily does
  // not support eBPF)
  bool HasEBPFSupport() {
    auto kernel = GetKernelVersion();
    return collector::HasEBPFSupport(kernel, GetOSID());
  }

  // Reads a named value from the os-release file (either in /etc/ or in /usr/lib)
  // and filters for a specific name. The file is in the format <NAME>="<VALUE>"
  // Quotes are removed from the value, if found. If not found, an empty string is returned.
  virtual std::string GetOSReleaseValue(const char* key);

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

  // Given a stream, reads line by line, expecting '<key>=<value>' format
  // returns the value matching the key called 'name'
  std::string filterForKey(std::istream& stream, const char* name);
};

}  // namespace collector

#endif  // _HOSTINFO_H
