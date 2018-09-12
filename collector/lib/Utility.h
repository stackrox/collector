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

extern "C" {
#include <errno.h>
}

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

namespace impl {

inline void StrAppend(std::ostringstream* os) {}

template <typename Arg1, typename... Args>
void StrAppend(std::ostringstream* os, Arg1&& arg1, Args&&... args) {
  *os << std::forward<Arg1>(arg1);
  StrAppend(os, std::forward<Args>(args)...);
}

}  // namespace impl

template <typename... Args>
std::string Str(Args&&... args) {
  std::ostringstream string_stream;
  impl::StrAppend(&string_stream, std::forward<Args>(args)...);
  return string_stream.str();
}

std::ostream& operator<<(std::ostream& os, const sinsp_threadinfo *t);

// UUIDStr returns UUID in string format.
const char* UUIDStr();

// Lock allows locking any mutex without having to explictly specify the type of the mutex, e.g.,
// auto lock = Lock(mutex_);
template <typename Mutex>
std::unique_lock<Mutex> Lock(Mutex& mutex) {
  return std::unique_lock<Mutex>(mutex);
}

#define SCOPED_LOCK(m) auto __scoped_lock_ ## __LINE__ = Lock(m)

}  // namespace collector

#endif  // _UTILITY_H_
