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

#include "DriverCandidates.h"

#include <filesystem>
#include <optional>
#include <string_view>

#include "HostInfo.h"
#include "SysdigService.h"
#include "Utility.h"

namespace collector {

namespace {

std::string driverFullName(const std::string& shortName) {
  return std::string{SysdigService::kProbeName} + "-" + shortName + ".o";
}

// Retrieves the ubuntu backport version from the host kernel's release
// string. If an appropriate backport version is not found, this function
// returns an empty std::optional.
std::optional<DriverCandidate> getUbuntuBackport(HostInfo& host) {
  static const char* candidates[] = {
      "~16.04",
      "~20.04",
  };

  auto kernel = host.GetKernelVersion();
  for (auto candidate : candidates) {
    if (kernel.version.find(candidate) != std::string::npos) {
      std::string backport = kernel.release + candidate;
      std::string name = driverFullName(backport);
      return DriverCandidate(std::move(name), EBPF);
    }
  }

  return {};
}

// Garden linux uses a special kernel version in order to avoid
// overlapping with Debian. This function returns the appropriate
// name for this candidate while keeping the possibility to use
// the Debian driver if it doesn't exist.
std::optional<DriverCandidate> getGardenLinuxCandidate(HostInfo& host) {
  auto kernel = host.GetKernelVersion();

  std::regex garden_linux_kernel_re(R"(\d+\.\d+\.\d+-\w+)");
  std::smatch match;

  if (!std::regex_search(kernel.version, match, garden_linux_kernel_re)) {
    CLOG(WARNING) << "Failed to match the Garden Linux kernel version.";
    return {};
  }

  // The Garden Linux specific candidate is of the form
  // 5.10.0-9-cloud-amd64-gl-5.10.83-1gardenlinux1
  std::string shortName = kernel.release + "-gl-" + match.str();
  std::string name = driverFullName(shortName);

  return DriverCandidate(name, EBPF);
}

// The kvm driver for minikube uses a custom kernel built from
// mostly vanilla kernel headers and its own configuration defined
// in their repo. However, when using the docker driver, minikube
// runs directly on the host, so we add the kvm kernel as a candidate
// in order to give the chance for collector to use the host driver.
std::optional<DriverCandidate> getMinikubeCandidate(HostInfo& host) {
  auto minikube_version = host.GetMinikubeVersion();

  if (minikube_version.empty()) {
    return {};
  }

  auto kernel = host.GetKernelVersion();

  std::string shortName = kernel.ShortRelease() + "-minikube-" + minikube_version;
  std::string name = driverFullName(shortName);
  return DriverCandidate(name, EBPF);
}

// Normalizes this host's release string into something collector can use
// to download appropriate kernel objects from the webserver. If the release
// string does not require normalization, it is simply returned.
std::string normalizeReleaseString(HostInfo& host) {
  auto kernel = host.GetKernelVersion();
  if (host.IsCOS()) {
    std::string release = kernel.release;
    // remove the + from the end of the kernel version
    if (release[release.size() - 1] == '+') {
      release.erase(release.size() - 1);
    }
    return release + "-" + host.GetBuildID() + "-" + host.GetOSID();
  }

  if (host.IsDockerDesktop()) {
    auto smp = kernel.version.find("SMP ");
    if (smp == std::string::npos) {
      CLOG(FATAL) << "Unable to parse docker desktop kernel version: "
                  << "'" << kernel.version << "'";
    }

    std::string time_string = kernel.version.substr(smp + 4);
    std::tm tm{};
    // Currently assuming that all docker desktop kernels have UTC timestamps
    // to simplify parsing for this edge case. std::get_time does not support parsing
    // timezone information (%Z)
    if (strptime(kernel.version.substr(smp + 4).c_str(), "%a %b %d %H:%M:%S UTC %Y", &tm) == nullptr) {
      CLOG(FATAL) << "Failed to parse DockerDesktop kernel timestamp: '" << time_string << "'";
    }
    std::stringstream timestamp;
    timestamp << std::put_time(&tm, "%Y-%m-%d-%H-%M-%S");
    return kernel.ShortRelease() + "-dockerdesktop-" + timestamp.str();
  }

  return kernel.release;
}

DriverCandidate getCoreBpfCandidate() {
  return DriverCandidate("CO.RE eBPF probe", CORE_BPF, false);
}

DriverCandidate getHostCandidate(HostInfo& host) {
  std::string hostCandidate = normalizeReleaseString(host);
  std::string hostCandidateFullName = driverFullName(hostCandidate);

  return DriverCandidate(hostCandidateFullName, EBPF);
}

DriverCandidate getUserDriverCandidate(const char* full_name) {
  std::filesystem::path driver_file(full_name);

  if (driver_file.is_absolute()) {
    return DriverCandidate(driver_file.filename(), EBPF, false, driver_file.parent_path());
  }

  return DriverCandidate(driver_file, EBPF, false);
}
}  // namespace

std::vector<DriverCandidate> GetKernelCandidates(CollectionMethod cm) {
  std::vector<DriverCandidate> candidates;

  const char* kernel_candidates = std::getenv("KERNEL_CANDIDATES");
  if (kernel_candidates && *kernel_candidates) {
    std::string_view sview(kernel_candidates);

    for (const auto& candidate_name : SplitStringView(sview)) {
      std::string name = driverFullName(candidate_name);
      candidates.emplace_back(std::move(name), EBPF);
    }

    return candidates;
  }

  const char* user_driver = std::getenv("COLLECTOR_DRIVER");
  if (user_driver && *user_driver) {
    candidates.push_back(getUserDriverCandidate(user_driver));
  }

  if (cm == CORE_BPF) {
    candidates.push_back(getCoreBpfCandidate());
  }

  HostInfo& host = HostInfo::Instance();

  if (host.IsUbuntu()) {
    auto backport = getUbuntuBackport(host);
    if (backport) {
      candidates.push_back(std::move(*backport));
    }
  }

  if (host.IsGarden()) {
    auto garden_candidate = getGardenLinuxCandidate(host);

    if (garden_candidate) {
      candidates.push_back(std::move(*garden_candidate));
    }
  }

  candidates.push_back(getHostCandidate(host));

  if (host.IsMinikube()) {
    auto minikube_candidate = getMinikubeCandidate(host);

    if (minikube_candidate) {
      candidates.push_back(std::move(*minikube_candidate));
    }
  }

  return candidates;
}

}  // namespace collector
