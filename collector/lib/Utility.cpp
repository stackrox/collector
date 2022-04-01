extern "C" {

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <uuid/uuid.h>
}

#include <fstream>
#include <regex>

#include "HostInfo.h"
#include "Logging.h"
#include "StringView.h"
#include "Utility.h"

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

  if (host.IsMinikube()) {
    return kernel.release + "-minikube";
  }

  return kernel.release;
}

}  // namespace

static constexpr int kMsgBufSize = 4096;

const char* StrError(int errnum) {
  thread_local char msg_buffer[kMsgBufSize];
  // If _GNU_SOURCE is defined, strerror_r is not the POSIX-compliant version returning an int and always storing
  // in the supplied buffer, but a thread-safe version returning a const char* (which may or may not alias with the
  // supplied buffer).
#if defined(_GNU_SOURCE) && _GNU_SOURCE
  return strerror_r(errnum, msg_buffer, kMsgBufSize);
#else
  strerror_r(errnum, msg_buffer, kMsgBufSize);
  return msg_buffer;
#endif
}

const char* SignalName(int signum) {
  switch (signum) {
#define SIG(name) \
  case SIG##name: \
    return "SIG" #name
    SIG(ABRT);
    SIG(SEGV);
    SIG(TERM);
    SIG(INT);
#undef SIG
    default:
      return "<unknown>";
  }
}

std::ostream& operator<<(std::ostream& os, const sinsp_threadinfo* t) {
  if (t) {
    os << "Container: \"" << t->m_container_id << "\", Name: " << t->m_comm << ", PID: " << t->m_pid << ", Args: " << t->m_exe;
  } else {
    os << "NULL\n";
  }
  return os;
}

const char* UUIDStr() {
  uuid_t uuid;
  constexpr int kUuidStringLength = 36;  // uuid_unparse manpage says so.
  thread_local char uuid_str[kUuidStringLength + 1];
  uuid_generate_time_safe(uuid);
  uuid_unparse_lower(uuid, uuid_str);

  return uuid_str;
}

static const std::string base64_chars =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

static inline bool is_base64(unsigned char c) {
  return (std::isalnum(c) || (c == '+') || (c == '/'));
}

std::string Base64Decode(std::string const& encoded_string) {
  int in_len = encoded_string.size();
  int i = 0;
  int j = 0;
  int in_ = 0;
  unsigned char char_array_4[4], char_array_3[3];
  std::string ret;

  while (in_len-- && (encoded_string[in_] != '=') && is_base64(encoded_string[in_])) {
    char_array_4[i++] = encoded_string[in_];
    in_++;
    if (i == 4) {
      for (i = 0; i < 4; i++)
        char_array_4[i] = base64_chars.find(char_array_4[i]);

      char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
      char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
      char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

      for (i = 0; (i < 3); i++)
        ret += char_array_3[i];
      i = 0;
    }
  }

  if (i) {
    for (j = i; j < 4; j++)
      char_array_4[j] = 0;

    for (j = 0; j < 4; j++)
      char_array_4[j] = base64_chars.find(char_array_4[j]);

    char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
    char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
    char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

    for (j = 0; (j < i - 1); j++) ret += char_array_3[j];
  }

  return ret;
}

std::string GetHostPath(const std::string& file) {
  const char* host_root = std::getenv("COLLECTOR_HOST_ROOT");
  if (!host_root) host_root = "";
  std::string host_file(host_root);
  // Check if we are joining paths without a seperator,
  if (host_file.length() && file.length() &&
      host_file.back() != '/' && file.front() != '/') {
    host_file += '/';
  }
  host_file += file;
  return host_file;
}

const char* GetSNIHostname() {
  const char* hostname = std::getenv("SNI_HOSTNAME");
  if (hostname && *hostname) return hostname;

  // if the environment variable is not defined, then default
  // to sensor.stackrox
  return "sensor.stackrox";
}

std::string GetHostname() {
  HostInfo& info = HostInfo::Instance();
  return info.GetHostname();
}

std::vector<std::string> GetKernelCandidates() {
  const char* kernel_candidates = std::getenv("KERNEL_CANDIDATES");
  if (kernel_candidates && *kernel_candidates) {
    StringView sview(kernel_candidates);
    return sview.split(' ');
  }

  HostInfo& host = HostInfo::Instance();
  std::vector<std::string> candidates;

  if (host.IsUbuntu()) {
    std::string backport = getUbuntuBackport(host);
    if (!backport.empty()) {
      candidates.push_back(backport);
    }
  }

  if (host.IsGarden()) {
    auto garden_candidate = getGardenLinuxCandidate(host);

    if (!garden_candidate.empty()) {
      candidates.push_back(garden_candidate);
    }
  }

  candidates.push_back(normalizeReleaseString(host));

  return candidates;
}

const char* GetModuleDownloadBaseURL() {
  const char* module_download_base_url = std::getenv("MODULE_DOWNLOAD_BASE_URL");
  if (module_download_base_url && *module_download_base_url) return module_download_base_url;

  CLOG(DEBUG) << "MODULE_DOWNLOAD_BASE_URL not set";
  return "";
}

const std::string kKernelModulesDir = "/kernel-modules";

std::string GetModuleVersion() {
  // This function is expected to be called a handful of times
  // during initialization. If this condition changes, consider
  // adding a lazy initialized static variable and prevent
  // reading MODULE_VERSION.txt on every call.
  std::ifstream file(kKernelModulesDir + "/MODULE_VERSION.txt");
  if (!file.is_open()) {
    CLOG(WARNING) << "Failed to open '" << kKernelModulesDir << "/MODULE_VERSION.txt'";
    return "";
  }

  static std::string module_version;
  getline(file, module_version);

  return module_version;
}

void TryUnlink(const char* path) {
  if (unlink(path) != 0) {
    CLOG(WARNING) << "Failed to unlink '" << path << "': " << StrError();
  }
}

}  // namespace collector
