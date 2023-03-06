#include "DriverCandidates.h"

#include <optional>
#include <string_view>

#include "HostInfo.h"
#include "StringView.h"
#include "SysdigService.h"

namespace collector {

namespace {

std::string driverFullName(const std::string& shortName, bool useEbpf) {
  if (useEbpf) {
    return std::string{SysdigService::kProbeName} + "-" + shortName + ".o";
  }
  return std::string{SysdigService::kModuleName} + "-" + shortName + ".ko";
}

// Retrieves the ubuntu backport version from the host kernel's release
// string. If the host is not Ubuntu, or it is unable to find an appropriate
// backport version, this function returns an empty string.
std::optional<DriverCandidate> getUbuntuBackport(HostInfo& host, bool useEbpf) {
  static const char* candidates[] = {
      "~16.04",
      "~20.04",
  };

  auto kernel = host.GetKernelVersion();
  for (auto candidate : candidates) {
    if (kernel.version.find(candidate) != std::string::npos) {
      std::string backport = kernel.release + candidate;
      std::string name = driverFullName(backport, useEbpf);
      return DriverCandidate(std::move(name), useEbpf, std::move(backport));
    }
  }

  return {};
}

// Garden linux uses a special kernel version in order to avoid
// overlapping with Debian. This function returns the appropriate
// name for this candidate while keeping the possibility to use
// the Debian driver if it doesn't exist.
std::optional<DriverCandidate> getGardenLinuxCandidate(HostInfo& host, bool useEbpf) {
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
  std::string name = driverFullName(shortName, useEbpf);

  return DriverCandidate(name, useEbpf, shortName);
}

// The kvm driver for minikube uses a custom kernel built from
// mostly vanilla kernel headers and its own configuration defined
// in their repo. However, when using the docker driver, minikube
// runs directly on the host, so we add the kvm kernel as a candidate
// in order to give the chance for collector to use the host driver.
std::optional<DriverCandidate> getMinikubeCandidate(HostInfo& host, bool useEbpf) {
  auto minikube_version = host.GetMinikubeVersion();

  if (minikube_version.empty()) {
    return {};
  }

  auto kernel = host.GetKernelVersion();

  std::string shortName = kernel.ShortRelease() + "-minikube-" + minikube_version;
  std::string name = driverFullName(shortName, useEbpf);
  return DriverCandidate(name, useEbpf, shortName);
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

DriverCandidate getHostCandidate(HostInfo& host, bool useEbpf) {
  std::string hostCandidate = normalizeReleaseString(host);
  std::string hostCandidateFullName = driverFullName(hostCandidate, useEbpf);

  return DriverCandidate(hostCandidateFullName, useEbpf, hostCandidate);
}

DriverCandidate getUserDriverCandidate(const char* full_name, bool useEbpf) {
  std::string_view path(full_name);

  // if the name starts with '/' we assume a fully qualified path was provided
  if (path.rfind("/", 0) == 0) {
    std::string_view::size_type last_separator = path.rfind('/');
    std::string name(path.substr(last_separator + 1));
    path.remove_suffix(path.size() - last_separator);

    return DriverCandidate(std::move(name), useEbpf, "", std::move(std::string(path)), false);
  }

  return DriverCandidate(full_name, useEbpf, false);
}
}  // namespace

std::vector<DriverCandidate> GetKernelCandidates(bool useEbpf) {
  std::vector<DriverCandidate> candidates;

  const char* kernel_candidates = std::getenv("KERNEL_CANDIDATES");
  if (kernel_candidates && *kernel_candidates) {
    StringView sview(kernel_candidates);

    for (const auto& candidate_name : sview.split(' ')) {
      std::string name = driverFullName(candidate_name, useEbpf);
      candidates.emplace_back(std::move(name), useEbpf, std::move(candidate_name));
    }

    return candidates;
  }

  const char* user_driver = std::getenv("COLLECTOR_DRIVER");
  if (user_driver && *user_driver) {
    candidates.push_back(getUserDriverCandidate(user_driver, useEbpf));
  }

  HostInfo& host = HostInfo::Instance();

  if (host.IsUbuntu()) {
    auto backport = getUbuntuBackport(host, useEbpf);
    if (backport) {
      candidates.push_back(std::move(*backport));
    }
  }

  if (host.IsGarden()) {
    auto garden_candidate = getGardenLinuxCandidate(host, useEbpf);

    if (garden_candidate) {
      candidates.push_back(std::move(*garden_candidate));
    }
  }

  candidates.push_back(getHostCandidate(host, useEbpf));

  if (host.IsMinikube()) {
    auto minikube_candidate = getMinikubeCandidate(host, useEbpf);

    if (minikube_candidate) {
      candidates.push_back(std::move(*minikube_candidate));
    }
  }

  return candidates;
}

}  // namespace collector
