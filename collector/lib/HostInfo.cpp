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

#include "HostInfo.h"

#include <fstream>

#include "Logging.h"

namespace collector {

namespace {

// Helper method which checks whether the given kernel & os
// are RHEL 7.6 (to inform later heuristics around eBPF support)
bool isRHEL76(const KernelVersion& kernel, const std::string& os_id) {
  if (os_id == "rhel" || os_id == "centos") {
    // example release version: 3.10.0-957.10.1.el7.x86_64
    // build_id = 957
    if (kernel.release.find(".el7.") != std::string::npos) {
      if (kernel.kernel == 3 && kernel.major == 10) {
        return kernel.build_id >= MIN_RHEL_BUILD_ID;
      }
    }
  }
  return false;
}

// Helper method which checks whether the given kernel & os
// support eBPF. In practice this is RHEL 7.6, and any kernel
// newer than 4.14
bool hasEBPFSupport(const KernelVersion& kernel, const std::string& os_id) {
  if (isRHEL76(kernel, os_id)) {
    return true;
  }
  return kernel.HasEBPFSupport();
}

// Given a stream, reads line by line, expecting '<key>=<value>' format
// returns the value matching the key called 'name'
std::string filterForKey(std::istream& stream, const char* name) {
  std::string line;
  while (std::getline(stream, line)) {
    auto idx = line.find('=');
    if (idx == std::string::npos) {
      continue;
    }
    std::string key = line.substr(0, idx);
    if (key == name) {
      std::string value = line.substr(idx + 1);
      // ensure we remove quotations from the start and end, if they exist.
      if (value[0] == '"') {
        value.erase(0, 1);
      }
      if (value[value.size() - 1] == '"') {
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
      std::string hostnameFile = GetHostPath("/etc/hostname");
      std::ifstream file(hostnameFile);
      if (!file.is_open()) {
        CLOG(INFO) << hostnameFile << " file not found";
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
    distro_ = GetOSReleaseValue("PRETTY_NAME");
    if (distro_.empty()) {
      distro_ = "Linux";
    }
  }
  return distro_;
}

std::string& HostInfo::GetBuildID() {
  if (build_id_.empty()) {
    build_id_ = GetOSReleaseValue("BUILD_ID");
  }
  return build_id_;
}

std::string& HostInfo::GetOSID() {
  if (os_id_.empty()) {
    os_id_ = GetOSReleaseValue("ID");
  }
  return os_id_;
}

std::string HostInfo::GetOSReleaseValue(const char* name) {
  std::ifstream release_file(GetHostPath("/etc/os-release"));
  if (!release_file.is_open()) {
    release_file.open(GetHostPath("/usr/lib/os-release"));
    if (!release_file.is_open()) {
      CLOG(ERROR) << "failed to open os-release file, unable to resolve OS information.";
      return "";
    }
  }

  return filterForKey(release_file, name);
}

bool HostInfo::IsRHEL76() {
  auto kernel = GetKernelVersion();
  return collector::isRHEL76(kernel, GetOSID());
}

bool HostInfo::HasEBPFSupport() {
  auto kernel = GetKernelVersion();
  return collector::hasEBPFSupport(kernel, GetOSID());
}

bool HostInfo::IsUEFI() {
  struct stat sb;
  std::string efi_path = GetHostPath("/sys/firmware/efi");

  if (stat(efi_path.c_str(), &sb) == -1) {
    if (errno == ENOTDIR || errno == ENOENT) {
      CLOG(INFO) << "Efi directory doesn't exist, legacy boot mode";
      return false;

    } else {
      CLOG(WARNING) << "Could not stat " << efi_path << ": " << StrError()
                    << ". No UEFI heuristic is performed.";
      return false;
    }
  }

  if (!S_ISDIR(sb.st_mode)) {
    CLOG(WARNING) << "Efi path is not a directory, legacy boot mode";
    return false;
  }

  CLOG(INFO) << "Efi directory exist, UEFI boot mode";
  return true;
}

SecureBootStatus HostInfo::HasSecureBoot() {
  std::uint8_t status;

  std::ifstream boot_params(
    GetHostPath("/sys/kernel/boot_params/data"),
    std::ios::binary | std::ios::in);

  if (!boot_params.is_open()) {
    CLOG(WARNING) << "Failed to open boot_params file.";
    return SecureBootStatus::NOT_DETERMINED;
  }

  boot_params.seekg(SECURE_BOOT_OFFSET);
  boot_params.read(reinterpret_cast<char*>(&status), 1);

  if (status < SecureBootStatus::NOT_DETERMINED ||
      status > SecureBootStatus::ENABLED) {
    CLOG(WARNING) << "Incorrect secure_boot param: " << status;
    return SecureBootStatus::NOT_DETERMINED;
  }

  return static_cast<SecureBootStatus>(status);
}

}  // namespace collector
