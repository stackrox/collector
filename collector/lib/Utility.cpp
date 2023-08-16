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
#include "Utility.h"

namespace collector {

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

std::vector<std::string> SplitStringView(const std::string_view sv, char delim) {
  std::vector<std::string> parts;
  std::string_view::size_type offset = 0;

  for (auto n = sv.find(delim); n != std::string_view::npos; n = sv.find(delim, offset)) {
    parts.push_back(std::string(sv.substr(offset, n - offset)));
    offset = n + 1;
  }

  // Push remainder of the string. This may be empty if the string
  // ends with a delimiter.
  parts.push_back(std::string(sv.substr(offset)));

  return parts;
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
    CLOG(ERROR) << "Failed to open '" << kKernelModulesDir << "/MODULE_VERSION.txt'";
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

// IsContainerID returns whether the given string view represents a container ID.
bool IsContainerID(std::string_view str) {
  if (str.size() != 64) return false;

  return std::all_of(str.begin(), str.end(), [](char c) -> bool {
    return std::isxdigit(c);
  });
}

std::optional<std::string_view> ExtractContainerIDFromCgroup(std::string_view cgroup) {
  if (cgroup.size() < 65) return {};

  auto scope = cgroup.rfind(".scope");
  if (scope != std::string_view::npos) {
    cgroup.remove_suffix(cgroup.length() - scope);
  }

  if (cgroup.rfind("-conmon-") != std::string_view::npos) {
    return {};
  }

  if (cgroup.size() < 65) return {};
  auto container_id_part = cgroup.substr(cgroup.size() - 65);
  if (container_id_part[0] != '/' && container_id_part[0] != '-') return {};

  container_id_part.remove_prefix(1);

  if (!IsContainerID(container_id_part)) return {};
  return std::make_optional(container_id_part.substr(0, 12));
}
}  // namespace collector
