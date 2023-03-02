#include "DriverCandidates.h"

#include "HostInfo.h"
#include "StringView.h"
#include "SysdigService.h"

namespace collector {

namespace {

// Retrieves the ubuntu backport version from the host kernel's release
// string. If the host is not Ubuntu, or it is unable to find an appropriate
// backport version, this function returns an empty string.
std::string getUbuntuBackport(HostInfo& host) {
  if (!host.IsUbuntu()) {
    return "";
  }

  static const char* candidates[] = {
      "~16.04",
      "~20.04",
  };

  auto kernel = host.GetKernelVersion();
  for (auto candidate : candidates) {
    if (kernel.version.find(candidate) != std::string::npos) {
      return kernel.release + candidate;
    }
  }

  return "";
}

// Garden linux uses a special kernel version in order to avoid
// overlapping with Debian. This function returns the appropriate
// name for this candidate while keeping the possibility to use
// the Debian driver if it doesn't exist.
std::string getGardenLinuxCandidate(HostInfo& host) {
  auto kernel = host.GetKernelVersion();

  std::regex garden_linux_kernel_re(R"(\d+\.\d+\.\d+-\w+)");
  std::smatch match;

  if (!std::regex_search(kernel.version, match, garden_linux_kernel_re)) {
    CLOG(WARNING) << "Failed to match the Garden Linux kernel version.";
    return "";
  }

  // The Garden Linux specific candidate is of the form
  // 5.10.0-9-cloud-amd64-gl-5.10.83-1gardenlinux1
  return kernel.release + "-gl-" + match.str();
}

// The kvm driver for minikube uses a custom kernel built from
// mostly vanilla kernel headers and its own configuration defined
// in their repo. However, when using the docker driver, minikube
// runs directly on the host, so we add the kvm kernel as a candidate
// in order to give the chance for collector to use the host driver.
std::string getMinikubeCandidate(HostInfo& host) {
  auto minikube_version = host.GetMinikubeVersion();

  if (minikube_version.empty()) {
    return "";
  }

  auto kernel = host.GetKernelVersion();

  return kernel.ShortRelease() + "-minikube-" + minikube_version;
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

std::string driverFullName(const std::string& shortName, bool useEbpf) {
  if (useEbpf) {
    return std::string{SysdigService::kProbeName} + "-" + shortName + ".o";
  }
  return std::string{SysdigService::kModuleName} + "-" + shortName + ".ko";
}

}  // namespace

std::vector<DriverCandidate> GetKernelCandidates(bool useEbpf) {
  std::vector<DriverCandidate> candidates;

  const char* kernel_candidates = std::getenv("KERNEL_CANDIDATES");
  if (kernel_candidates && *kernel_candidates) {
    StringView sview(kernel_candidates);

    for (const auto& candidate_name : sview.split(' ')) {
      std::string name = driverFullName(candidate_name, useEbpf);
      candidates.emplace_back(std::move(name), std::move(candidate_name));
    }

    return candidates;
  }

  HostInfo& host = HostInfo::Instance();

  if (host.IsUbuntu()) {
    std::string backport = getUbuntuBackport(host);
    if (!backport.empty()) {
      std::string name = driverFullName(backport, useEbpf);
      candidates.emplace_back(std::move(name), std::move(backport));
    }
  }

  if (host.IsGarden()) {
    auto garden_candidate = getGardenLinuxCandidate(host);

    if (!garden_candidate.empty()) {
      std::string name = driverFullName(garden_candidate, useEbpf);
      candidates.emplace_back(std::move(name), std::move(garden_candidate));
    }
  }

  std::string hostCandidate = normalizeReleaseString(host);
  std::string hostCandidateFullName = driverFullName(hostCandidate, useEbpf);
  candidates.emplace_back(std::move(hostCandidateFullName), std::move(hostCandidate));

  if (host.IsMinikube()) {
    auto minikube_candidate = getMinikubeCandidate(host);

    if (!minikube_candidate.empty()) {
      std::string name = driverFullName(minikube_candidate, useEbpf);
      candidates.emplace_back(std::move(name), std::move(minikube_candidate));
    }
  }

  return candidates;
}

}  // namespace collector
