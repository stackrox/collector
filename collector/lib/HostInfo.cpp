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

// An offset for secure_boot option in boot_params.
// See https://www.kernel.org/doc/html/latest/x86/zero-page.html
const int SECURE_BOOT_OFFSET = 0x1EC;

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

const std::string GetHostnameFromFile(const std::string& hostnamePath) {
  std::string hostnameFile = GetHostPath(hostnamePath);
  std::ifstream file(hostnameFile);
  std::string hostname = "";
  if (!file.is_open()) {
    CLOG(DEBUG) << hostnameFile << " file not found";
    CLOG(DEBUG) << "Failed to determine hostname from " << hostnameFile;
  } else if (!std::getline(file, hostname)) {
    CLOG(DEBUG) << hostnameFile << " is empty";
    CLOG(DEBUG) << "Failed to determine hostname from " << hostnameFile;
  }
  return hostname;
}

}  // namespace

KernelVersion HostInfo::GetKernelVersion() {
  if (kernel_version_.release.empty()) {
    kernel_version_ = KernelVersion::FromHost();
  }
  return kernel_version_;
}

const std::string& HostInfo::GetHostname() {
  if (hostname_.empty()) {
    const char* hostname_env = std::getenv("NODE_HOSTNAME");
    if (hostname_env && *hostname_env) {
      hostname_ = std::string(hostname_env);
      CLOG(INFO) << "Environment variable NODE_HOSTNAME is set to " << hostname_;
    } else {
      // if we can't get the hostname from the environment
      // we can look in /etc or /proc (mounted at /host/etc or /host/proc in the collector container)
      std::vector<std::string> hostnamePaths{"/etc/hostname", "/proc/sys/kernel/hostname"};
      for (auto hostnamePath : hostnamePaths) {
        hostname_ = GetHostnameFromFile(hostnamePath);
        if (!hostname_.empty()) break;
      }
    }
    CLOG(INFO) << "Hostname: '" << hostname_ << "'";
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
      CLOG(DEBUG) << "EFI directory doesn't exist, legacy boot mode";
      return false;

    } else {
      CLOG(WARNING) << "Could not stat " << efi_path << ": " << StrError()
                    << ". No UEFI heuristic is performed.";
      return false;
    }
  }

  if (!S_ISDIR(sb.st_mode)) {
    CLOG(WARNING) << "EFI path is not a directory, legacy boot mode";
    return false;
  }

  CLOG(DEBUG) << "EFI directory exist, UEFI boot mode";
  return true;
}

// Get SecureBoot status from reading a corresponding EFI variable. Every such
// variable is a small file <key name>-<vendor-guid> in efivarfs directory, and
// its format is described in UEFI specification.
SecureBootStatus HostInfo::GetSecureBootFromVars() {
  std::uint8_t status;
  std::string efi_path = GetHostPath("/sys/firmware/efi/efivars");
  DirHandle efivars = opendir(efi_path.c_str());

  if (!efivars.valid()) {
    CLOG(INFO) << "Could not open " << efi_path << ": " << StrError();
    return SecureBootStatus::NOT_DETERMINED;
  }

  while (auto dp = efivars.read()) {
    std::string name(dp->d_name);

    if (name.rfind("SecureBoot-", 0) == 0) {
      std::uint8_t efi_key[5];
      std::string path = efi_path + "/" + name;

      // There should be only one SecureBoot key, so it doesn't make sense to
      // search further in case if e.g. it couldn't be read.
      std::ifstream secure_boot(path, std::ios::binary | std::ios::in);
      if (!secure_boot.is_open()) {
        CLOG(WARNING) << "Failed to open SecureBoot key " << path;
        return SecureBootStatus::NOT_DETERMINED;
      }

      // An EFI variable contains 4 bytes with attributes, and 5th with the
      // actual value. The efivarfs doesn't support lseek, returning ESPIPE on
      // it, so read the header first, then the actual value.
      // See https://www.kernel.org/doc/html/latest/filesystems/efivarfs.html
      secure_boot.read(reinterpret_cast<char*>(&efi_key), 5);
      status = efi_key[4];

      // Pretty intuitively 0 means the feature is disabled, 1 enabled.
      // SecureBoot efi variable doesn't have NOT_DETERMINED value.
      // See https://uefi.org/sites/default/files/resources/UEFI_Spec_2_9_2021_03_18.pdf#page=86
      if (status != 0 && status != 1) {
        CLOG(WARNING) << "Incorrect secure_boot param: " << (unsigned int)status;
        return SecureBootStatus::NOT_DETERMINED;
      }

      return static_cast<SecureBootStatus>(status);
    }
  }

  // No SecureBoot key found
  return SecureBootStatus::NOT_DETERMINED;
}

// Get SecureBoot status from reading boot_params structure. Not only it will
// tell whether the SecureBoot is enabled or disabled, but also if could not be
// determined.
SecureBootStatus HostInfo::GetSecureBootFromParams() {
  std::uint8_t status;
  std::string boot_params_path = GetHostPath("/sys/kernel/boot_params/data");

  std::ifstream boot_params(boot_params_path, std::ios::binary | std::ios::in);

  if (!boot_params.is_open()) {
    CLOG(WARNING) << "Failed to open " << boot_params_path;
    return SecureBootStatus::NOT_DETERMINED;
  }

  boot_params.seekg(SECURE_BOOT_OFFSET);
  boot_params.read(reinterpret_cast<char*>(&status), 1);

  if (status < SecureBootStatus::NOT_DETERMINED ||
      status > SecureBootStatus::ENABLED) {
    CLOG(WARNING) << "Incorrect secure_boot param: " << (unsigned int)status;
    return SecureBootStatus::NOT_DETERMINED;
  }

  return static_cast<SecureBootStatus>(status);
}

SecureBootStatus HostInfo::GetSecureBootStatus() {
  std::uint8_t status;
  auto kernel = GetKernelVersion();

  if (secure_boot_status_ != SecureBootStatus::UNSET) {
    return secure_boot_status_;
  }

  if (kernel.HasSecureBootParam()) {
    status = GetSecureBootFromParams();
  } else {
    status = GetSecureBootFromVars();
  }

  secure_boot_status_ = static_cast<SecureBootStatus>(status);

  CLOG(DEBUG) << "SecureBoot status is " << secure_boot_status_;
  return secure_boot_status_;
}

}  // namespace collector
