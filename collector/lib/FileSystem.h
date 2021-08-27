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

#ifndef COLLECTOR_FILESYSTEM_H
#define COLLECTOR_FILESYSTEM_H

#include <cstdio>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <zlib.h>

namespace collector {

// ResourceWrapper wraps an operating system resource to provide RAII-style lifetime management. T is the type of the
// resource being wrapped (e.g., int for file descriptors), and Derived is the type extending this class. The latter
// has to provide static methods `T Invalid()`, returning the value of T for an invalid resource, and `bool Close(T)`
// to close the resource.
template <typename T, typename Derived>
class ResourceWrapper {
 public:
  ResourceWrapper() : resource_(Derived::Invalid()) {}
  ResourceWrapper(T&& resource) : resource_(resource) {}
  ~ResourceWrapper() {
    close();
  }

  bool valid() const { return resource_ != Derived::Invalid(); }

  const T& get() const { return resource_; }
  T release() {
    T result = resource_;
    resource_ = Derived::Invalid();
    return result;
  }

  bool close() {
    if (resource_ == Derived::Invalid()) {
      return true;
    }
    bool success = Derived::Close(resource_);
    resource_ = Derived::Invalid();
    return success;
  }

  Derived& operator=(T&& resource) {
    close();
    resource_ = resource;
    return *static_cast<Derived*>(this);
  }

  Derived& operator=(Derived&& other) {
    close();
    resource_ = other.resource_;
    other.resource_ = Derived::Invalid();
    return *static_cast<Derived*>(this);
  }

  operator T() const& { return get(); }
  operator T() && { return release(); }
  explicit operator bool() const { return valid(); }

 private:
  T resource_;
};

class FDHandle : public ResourceWrapper<int, FDHandle> {
 public:
  using ResourceWrapper::ResourceWrapper;
  FDHandle(FDHandle&& other) : ResourceWrapper(other.release()) {}

  static constexpr int Invalid() { return -1; }
  static bool Close(int fd) { return (::close(fd) == 0); }
};

class FileHandle : public ResourceWrapper<std::FILE*, FileHandle> {
 public:
  using ResourceWrapper::ResourceWrapper;
  FileHandle(FileHandle&& other) : ResourceWrapper(other.release()) {}
  FileHandle(FDHandle&& fd, const char* mode) : ResourceWrapper(fdopen(fd.release(), mode)) {}

  static constexpr std::FILE* Invalid() { return nullptr; }
  static bool Close(std::FILE* f) { return (fclose(f) == 0); }
};

class DirHandle : public ResourceWrapper<DIR*, DirHandle> {
 public:
  using ResourceWrapper::ResourceWrapper;
  DirHandle(DirHandle&& other) : ResourceWrapper(other.release()) {}
  DirHandle(FDHandle&& fd) : ResourceWrapper(fdopendir(fd.release())) {}

  FDHandle openat(const char* path, int mode) const {
    if (!valid()) return -1;
    return ::openat(dirfd(get()), path, mode);
  }

  int fd() const { return ::dirfd(get()); }

  struct dirent* read() {
    if (!valid()) return nullptr;
    return readdir(get());
  }

  static constexpr DIR* Invalid() { return nullptr; }
  static bool Close(DIR* dir) { return (closedir(dir) == 0); }
};

class GZFileHandle : public ResourceWrapper<gzFile, GZFileHandle> {
 private:
  std::string error_msg_;

 public:
  using ResourceWrapper::ResourceWrapper;
  GZFileHandle(GZFileHandle&& other) : ResourceWrapper(other.release()) {}
  GZFileHandle(FDHandle&& fd, const char* mode) : ResourceWrapper(gzdopen(fd.release(), mode)) {}

  static constexpr gzFile Invalid() { return nullptr; }
  static bool Close(gzFile file) { return (gzclose(file) == Z_OK); }

  const std::string& error_msg() {
    int errnum = Z_OK;
    const char* gz_error = gzerror(get(), &errnum);

    if (gz_error == nullptr) {
      error_msg_ = "";
      return error_msg_;
    }

    error_msg_ = gz_error;

    if (errnum == Z_ERRNO) {
      error_msg_ + " - " + strerror(errno) + " (" + std::to_string(errno) + ")";
    }

    return error_msg_;
  };
};

}  // namespace collector

#endif  //COLLECTOR_FILESYSTEM_H
