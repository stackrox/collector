
#ifndef _UTILITY_H_
#define _UTILITY_H_

#include <cerrno>
#include <chrono>
#include <cinttypes>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <utility>

#include "libsinsp/sinsp.h"

namespace collector {

// The following functions are thread-safe versions of strerror, which are more convenient to use than strerror_r.
const char* StrError(int errnum = errno);

// Return the name of a signal. This function is reentrant.
const char* SignalName(int signum);

template <typename T, typename... Args>
std::unique_ptr<T> MakeUnique(Args&&... args) {
  return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

// Return string decoded from base 64
std::string Base64Decode(std::string const& encoded_string);

// Get path using host prefix from SYSDIG_HOST_ROOT env var
std::string GetHostPath(const std::string& file);

// Get SNI hostname from SNI_HOSTNAME env var
const char* GetSNIHostname();

// Get hostname from NODE_HOSTNAME env var
std::string GetHostname();

// Get kernel candidates from KERNEL_CANDIDATES env var
std::vector<std::string> GetKernelCandidates();

// Get base URL for kernel modules downloads from MODULE_DOWNLOAD_BASE_URL
const char* GetModuleDownloadBaseURL();

// Get the module version used by collector.
std::string GetModuleVersion();

// Wrapper around unlink(2) to handle error conditions.
void TryUnlink(const char* path);

namespace impl {

inline void StrAppend(std::ostringstream* os) {}

template <typename Arg1, typename... Args>
void StrAppend(std::ostringstream* os, Arg1&& arg1, Args&&... args) {
  *os << std::forward<Arg1>(arg1);
  StrAppend(os, std::forward<Args>(args)...);
}

}  // namespace impl

// Static variant of strlen
template <std::size_t N>
constexpr std::size_t StrLen(const char (&static_str)[N]) {
  return N - 1;
}

template <typename... Args>
std::string Str(Args&&... args) {
  std::ostringstream string_stream;
  impl::StrAppend(&string_stream, std::forward<Args>(args)...);
  return string_stream.str();
}

std::ostream& operator<<(std::ostream& os, const sinsp_threadinfo* t);

// UUIDStr returns UUID in string format.
const char* UUIDStr();

namespace internal {

// ScopedLock just wraps a `std::unique_lock`. In addition, its operator bool() is marked constexpr and always returns
// true, such that it can be used in an assignment in an `if` condition (see WITH_LOCK below).
template <typename Mutex>
class ScopedLock {
 public:
  ScopedLock(Mutex& m) : lock_(m) {}

  constexpr operator bool() const { return true; }

 private:
  std::unique_lock<Mutex> lock_;
};

// Lock allows locking any mutex without having to explictly specify the type of the mutex, such that its return value
// can just be assigned to an auto-typed variable.
template <typename Mutex>
ScopedLock<Mutex> Lock(Mutex& mutex) {
  return ScopedLock<Mutex>(mutex);
}

}  // namespace internal

#define WITH_LOCK(m) if (auto __scoped_lock_##__LINE__ = internal::Lock(m))

// ssizeof(x) returns the same value as sizeof(x), but as a signed integer.
#define ssizeof(x) static_cast<ssize_t>(sizeof(x))

extern const std::string kKernelModulesDir;

}  // namespace collector

#endif  // _UTILITY_H_
