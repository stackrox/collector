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
#ifndef _UTILITY_H_
#define _UTILITY_H_

#include <cerrno>
#include <cinttypes>

#include <chrono>
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

// Get hostname from NODE_HOSTNAME env var
const char* GetHostname();

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

std::ostream& operator<<(std::ostream& os, const sinsp_threadinfo *t);

constexpr int kUuidStringLength = 36;  // uuid_unparse manpage says so.

// UUIDStr returns UUID in string format.
void UUIDStr(char (&uuid_str)[kUuidStringLength + 1]);

namespace internal {

// ScopedLock just wraps a `std::unique_lock`. In addition, its operator bool() is marked constexpr and always returns
// true, such that it can be used in an assignment in an `if` condition (see WITH_LOCK below).
template<typename Mutex>
class ScopedLock {
 public:
  ScopedLock(Mutex &m) : lock_(m) {}

  constexpr operator bool() const { return true; }

 private:
  std::unique_lock<Mutex> lock_;
};

// Lock allows locking any mutex without having to explictly specify the type of the mutex, such that its return value
// can just be assigned to an auto-typed variable.
template<typename Mutex>
ScopedLock<Mutex> Lock(Mutex &mutex) {
  return ScopedLock<Mutex>(mutex);
}

}  // namespace internal

#define WITH_LOCK(m) if (auto __scoped_lock_ ## __LINE__ = internal::Lock(m))

// ssizeof(x) returns the same value as sizeof(x), but as a signed integer.
#define ssizeof(x) static_cast<ssize_t>(sizeof(x))

}  // namespace collector

#endif  // _UTILITY_H_
