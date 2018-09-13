//
// Created by Malte Isberner on 9/12/18.
//

#ifndef COLLECTOR_FILESYSTEM_H
#define COLLECTOR_FILESYSTEM_H

#include <dirent.h>
#include <unistd.h>

#include <cstdio>

namespace collector {

class FDHandle {
 public:
  FDHandle() : fd_(-1) {}
  FDHandle(int&& fd) : fd_(fd) {
    fd = -1;
  }

  FDHandle(FDHandle&& other) : fd_(other.fd_) {
    other.fd_ = -1;
  }

  ~FDHandle() {
    if (valid()) {
      close(fd_);
    }
  }

  int release() {
    int fd = fd_;
    fd_ = -1;
    return fd;
  }

  bool valid() const {
    return fd_ >= 0;
  }

  operator int() const {
    return fd_;
  }

  FDHandle& operator=(FDHandle&& other) {
    if (valid()) {
      close(fd_);
    }
    fd_ = other.fd_;
    other.fd_ = -1;

    return *this;
  }

 private:
  int fd_;
};

class FileHandle {
 public:
  FileHandle() : f_(nullptr) {}
  FileHandle(FILE*&& f) : f_(f) {
    f = nullptr;
  }
  FileHandle(FileHandle&& other) : f_(other.f_) {
    other.f_ = nullptr;
  }

  FileHandle(FDHandle&& fd_handle, const char* mode) : f_(fdopen(fd_handle.release(), mode)) {}

  ~FileHandle() {
    close();
  }

  bool close() {
    if (!f_) return true;
    bool status = (fclose(f_) == 0);
    f_ = nullptr;
    return status;
  }

  operator std::FILE*() const { return f_; }

  FileHandle& operator=(FileHandle&& other) {
    close();
    f_ = other.f_;
    other.f_ = nullptr;
    return *this;
  }

 private:
  std::FILE* f_;
};

class DirHandle {
 public:
  DirHandle() : dir_(nullptr) {}
  DirHandle(DIR*&& dir) : dir_(dir) {
    dir = nullptr;
  }
  DirHandle(FDHandle&& fd) : dir_(fdopendir(fd.release())) {}
  DirHandle(const char* path) : dir_(opendir(path)) {}

  DirHandle(DirHandle&& other) : dir_(other.dir_) {
    other.dir_ = nullptr;
  }

  ~DirHandle() {
    closedir(dir_);
  }

  struct dirent* read() {
    if (!dir_) return nullptr;
    return readdir(dir_);
  }

  DIR* release() {
    DIR* dir = dir_;
    dir_ = nullptr;
    return dir;
  }

  operator DIR*() const { return dir_; }

  bool valid() const { return dir_ != nullptr; }
  explicit operator bool() const { return valid(); }

  DirHandle& operator=(DirHandle&& other) {
    if (valid()) {
      closedir(dir_);
    }
    dir_ = other.dir_;
    other.dir_ = nullptr;
    return *this;
  }

 private:
  DIR* dir_;
};

}  // namespace collector

#endif //COLLECTOR_FILESYSTEM_H
