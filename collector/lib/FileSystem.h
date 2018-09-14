//
// Created by Malte Isberner on 9/12/18.
//

#ifndef COLLECTOR_FILESYSTEM_H
#define COLLECTOR_FILESYSTEM_H

#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>

#include <cstdio>

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
  FileHandle(FileHandle &&other) : ResourceWrapper(other.release()) {}
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

}  // namespace collector

#endif //COLLECTOR_FILESYSTEM_H
