#include "KernelDriver.h"

#include "FileSystem.h"
#include "SysdigService.h"
#include "Utility.h"

extern "C" {
#include <sys/stat.h>
}

#define init_module(module_image, len, param_values) syscall(__NR_init_module, module_image, len, param_values)
#define delete_module(name, flags) syscall(__NR_delete_module, name, flags)

namespace collector {

namespace {

int read_module(int fd, void* buf, int buflen) {
  unsigned char* p = static_cast<unsigned char*>(buf);
  int n, i = 0;
  while (i < buflen) {
    n = read(fd, p + i, buflen - i);
    if (n < 0) {
      if (errno == EINTR)
        continue;
      else
        return n;
    } else if (n == 0) {
      return i;
    } else {
      i += n;
    }
  }
  return i;
}

std::string constructArgsStr(std::unordered_map<std::string, std::string>& args) {
  bool first = true;
  std::stringstream stream;

  for (auto& entry : args) {
    if (first) {
      first = false;
    } else {
      stream << " ";
    }
    stream << entry.first << "=" << entry.second;
  }

  return stream.str();
}

int doInsertModule(int fd, std::unordered_map<std::string, std::string>& args) {
  std::string args_str = constructArgsStr(args);
  CLOG(DEBUG) << "Kernel module arguments: " << args_str;

  struct stat st;
  int res = fstat(fd, &st);
  if (res != 0) {
    CLOG(ERROR) << "Could not stat kernel module: " << StrError();
    errno = EINVAL;
    return -1;
  }
  size_t image_size = st.st_size;
  void* image = malloc(image_size);
  if (!image) {
    CLOG(ERROR) << "Could not allocate memory for kernel module: " << StrError();
    errno = EINVAL;
    return -1;
  }
  lseek(fd, 0, SEEK_SET);
  size_t read_image_size = read_module(fd, image, image_size);
  if (read_image_size != image_size) {
    CLOG(ERROR) << "Could not read kernel module: " << StrError() << ".  Mismatch with number of bytes read and kernel module size.";
    errno = EINVAL;
    return -1;
  }

  int res = init_module(fd, args_str.c_str(), 0);
  if (res != 0) {
    return res;
  }

  std::string param_dir = GetHostPath(
      std::string("/sys/module/") + SysdigService::kModuleName + "/parameters");

  if (stat(param_dir.c_str(), &st) != 0) {
    // This is not optimal, but don't fail hard on systems where
    // for whatever reason the above directory does not exist.
    CLOG(WARNING) << "Could not stat " << param_dir << ": " << StrError()
                  << ". No parameter verification can be performed.";
    return 0;
  }

  for (const auto& entry : args) {
    std::string param_file = param_dir + entry.first;

    if (stat(param_file.c_str(), &st) != 0) {
      CLOG(ERROR) << "Could not stat " << param_file << ": " << StrError()
                  << ". Parameter " << entry.first
                  << " is unsupported, suspecting module version mismatch.";
      errno = EINVAL;
      return -1;
    }
  }
  return 0;
}

}  // namespace

bool KernelDriverModule::insert(const std::vector<std::string>& syscalls, std::string path) {
  std::unordered_map<std::string, std::string> module_args;
  std::stringstream syscall_ids;

  // Iterate over the syscalls that we want and pull each of their ids.
  // These are stashed into a string that will get passed to init_module
  // to insert the kernel module
  const EventNames& event_names = EventNames::GetInstance();
  for (const auto& syscall : syscalls) {
    for (ppm_event_type id : event_names.GetEventIDs(syscall)) {
      syscall_ids << id << ",";
    }
  }
  syscall_ids << "-1";

  module_args["s_syscallIds"] = syscall_ids.str();
  module_args["exclude_initns"] = "1";
  module_args["exclude_selfns"] = "1";
  module_args["verbose"] = "0";

  FDHandle fd = FDHandle(open(path.c_str(), O_RDONLY));
  if (!fd.valid()) {
    CLOG(ERROR) << "Cannot open kernel module: " << path << ". Aborting...";
    return false;
  }

  CLOG(INFO) << "Inserting kernel module " << SysdigService::kModulePath
             << " with indefinite removal and retry if required.";

  // Attempt to insert the module. If it is already inserted then remove it and
  // try again
  int result = doInsertModule(fd.get(), module_args);
  while (result != 0) {
    if (errno == EEXIST) {
      // note that we forcefully remove the kernel module whether or not it has a non-zero
      // reference count. There is only one container that is ever expected to be using
      // this kernel module and that is us
      delete_module(SysdigService::kModuleName, O_NONBLOCK | O_TRUNC);
      sleep(2);  // wait for 2s before trying again
    } else {
      CLOG(ERROR) << "Error inserting kernel module: " << path << ": " << StrError()
                  << ". Aborting...";
      return false;
    }
    result = doInsertModule(fd.get(), module_args);
  }

  CLOG(INFO) << "Successfully inserted kernel module " << path << ".";
  return true;
}

}  // namespace collector
