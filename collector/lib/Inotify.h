#ifndef _INOTIFY_H_
#define _INOTIFY_H_

#include <array>
#include <cstdint>
#include <filesystem>
#include <ostream>
#include <string>
#include <unistd.h>
#include <variant>

#include <sys/inotify.h>

#include "Logging.h"
#include "Utility.h"

namespace collector {

class InotifyError : public std::exception {
 public:
  InotifyError(const std::string& error) : msg_(error) {}
  InotifyError(std::string&& error) : msg_(error) {}

  const char* what() const noexcept override { return msg_.c_str(); }

 private:
  std::string msg_;
};

class InotifyTimeout : public InotifyError {
 public:
  InotifyTimeout(const std::string& error) : InotifyError(error) {}
  InotifyTimeout(std::string&& error) : InotifyError(error) {}
};

using InotifyResult = std::variant<const struct inotify_event*, InotifyError, InotifyTimeout>;

enum InotifyResultVariants {
  INOTIFY_OK = 0,
  INOTIFY_ERROR,
  INOTIFY_TIMEOUT,
  INOTIFY_MAX,
};

static_assert(std::variant_size_v<InotifyResult> == INOTIFY_MAX);
static_assert(std::is_same_v<const inotify_event*, std::variant_alternative_t<INOTIFY_OK, InotifyResult>>);
static_assert(std::is_same_v<InotifyError, std::variant_alternative_t<INOTIFY_ERROR, InotifyResult>>);
static_assert(std::is_same_v<InotifyTimeout, std::variant_alternative_t<INOTIFY_TIMEOUT, InotifyResult>>);

struct Watcher {
  Watcher(int wd, std::filesystem::path path, int tag)
      : wd(wd), path(std::move(path)), tag(tag) {}

  int wd;
  std::filesystem::path path;
  int tag;
};
std::ostream& operator<<(std::ostream& os, const Watcher& w);

class Inotify {
 public:
  Inotify() : fd_(inotify_init()) {
    if (fd_ < 0) {
      CLOG(ERROR) << "Failed to initialize inotify: (" << errno << ") " << StrError();
    }
  }
  Inotify(const Inotify&) = delete;
  Inotify(Inotify&&) = delete;
  Inotify& operator=(const Inotify&) = delete;
  Inotify& operator=(Inotify&&) = delete;

  ~Inotify() {
    for (const auto& w : watchers_) {
      inotify_rm_watch(fd_, w.wd);
    }
    close(fd_);
  }

  bool IsValid() const {
    return fd_ > 0;
  }

  using WatcherStore = std::vector<Watcher>;
  using WatcherIterator = WatcherStore::iterator;
  int AddWatcher(const std::filesystem::path& p, uint32_t flags, int tag);
  int AddFileWatcher(const std::filesystem::path& p, int tag = 0);
  int AddDirectoryWatcher(const std::filesystem::path& p, int tag = 0);
  int RemoveWatcher(int wd);
  int RemoveWatcher(const std::filesystem::path& p);
  int RemoveWatcher(WatcherIterator it);
  WatcherIterator FindWatcher(const std::filesystem::path& needle);
  WatcherIterator FindWatcher(int needle);
  bool WatcherNotFound(WatcherIterator w) { return w == watchers_.end(); }
  InotifyResult GetNext();
  static std::string MaskToString(uint32_t mask);

 private:
  int fd_;
  std::array<char, 1024> buffer_{};
  size_t curr_byte_{0};
  size_t read_data_{0};

  // Watchers should probably be organized in a map. However,
  // sometimes we'll want to look for these with a path, sometimes
  // with a wd, so instead of over complicating things and assuming we
  // will only have a couple watchers for now, we just do a vector and
  // find by iterating. If we ever need more than just a couple, we
  // MUST change this.
  WatcherStore watchers_;

  friend std::ostream& operator<<(std::ostream& os, const Inotify& inotify) {
    bool first = true;
    for (const auto& w : inotify.watchers_) {
      if (first) {
        first = false;
      } else {
        os << "\n";
      }

      os << w;
    }
    return os;
  }

  static constexpr std::array<unsigned int, 17> BIT_MASKS{
      IN_ACCESS,
      IN_MODIFY,
      IN_ATTRIB,
      IN_CLOSE_WRITE,
      IN_CLOSE_NOWRITE,
      IN_OPEN,
      IN_MOVED_FROM,
      IN_MOVED_TO,
      IN_CREATE,
      IN_DELETE,
      IN_DELETE_SELF,
      IN_MOVE_SELF,
      IN_UNMOUNT,
      IN_Q_OVERFLOW,
      IN_IGNORED,
      IN_ISDIR,
      IN_ONESHOT,
  };

  static constexpr std::array<const char*, 17> MASK_STR{
      "access",
      "modify",
      "attrib",
      "close_write",
      "close_nowrite",
      "open",
      "moved_from",
      "moved_to",
      "create",
      "delete",
      "delete_self",
      "move_self",
      "unmount",
      "q_overflow",
      "ignored",
      "isdir",
      "oneshot",
  };

  static_assert(BIT_MASKS.size() == MASK_STR.size());
};

}  // namespace collector

#endif  // _INOTIFY_H_
