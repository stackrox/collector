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

#include "log/Logging.h"
#include "utils/Utility.h"

namespace collector {

/// Signal a system error prevented operations with inotify.
class InotifyError : public std::exception {
 public:
  InotifyError(const std::string& error) : msg_(error) {}
  InotifyError(std::string&& error) : msg_(error) {}

  const char* what() const noexcept override { return msg_.c_str(); }

 private:
  std::string msg_;
};

/// Reading an inotify event was not possible in the allocated time.
class InotifyTimeout : public InotifyError {
 public:
  InotifyTimeout(const std::string& error) : InotifyError(error) {}
  InotifyTimeout(std::string&& error = "'select' timedout") : InotifyError(error) {}
};

/// Value returned when reading an inotify event is attempted
using InotifyResult = std::variant<const struct inotify_event*, InotifyError, InotifyTimeout>;

/// Helper enum, enabling `switch(InotifyResult.index())`
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

/**
 * Helper struct for tracking watch descriptor <-> path mappings
 */
struct Watcher {
  Watcher(int wd, std::filesystem::path path, int tag)
      : wd(wd), path(std::move(path)), tag(tag) {}

  int wd;
  std::filesystem::path path;
  int tag;
};
std::ostream& operator<<(std::ostream& os, const Watcher& w);

/**
 * Abstraction over the inotify subsystem.
 */
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

  /**
   * Add an inotify watcher to the specified path.
   *
   * @param p A path to be monitored with inotify.
   * @param flags The inotify flags to be used. See `man inotify` for
   *              details.
   * @param tag An integer that can be used for easily correlating a wd
   *            to a path.
   * @returns A watch descriptor as described in `man inotify_add_watch`
   */
  int AddWatcher(const std::filesystem::path& p, uint32_t flags, int tag);

  /**
   * Convenience method for adding watchers to a file.
   *
   * @param p A path to be monitored with inotify.
   * @param tag An integer that can be used for easily correlating a wd to
   *            a path.
   * @returns A watch descriptor as described in `man inotify_add_watch`
   */
  int AddFileWatcher(const std::filesystem::path& p, int tag = 0);

  /**
   * Convenience method for adding watchers to a directory.
   *
   * @param p A path to be monitored with inotify.
   * @param tag An integer that can be used for easily correlating a wd to
   *            a path.
   * @returns A watch descriptor as described in `man inotify_add_watch`
   */
  int AddDirectoryWatcher(const std::filesystem::path& p, int tag = 0);

  /**
   * Remove a watch descriptor, stopping inotify from generating more
   * events for it.
   *
   * @param wd The watch descriptor to be removed.
   * @returns 0 on success, -1 otherwise.
   */
  int RemoveWatcher(int wd);

  /**
   * Remove a watch descriptor associated to the given path, stopping
   * inotify from generating more events for it.
   *
   * @param p The monitored path inotify should stop monitoring.
   * @returns 0 on success, -1 otherwise.
   */
  int RemoveWatcher(const std::filesystem::path& p);

  /**
   * Remove a watch descriptor associated to the given iterator,
   * stopping inotify from generating more events for it.
   *
   * Generally, you will want to use one of the RemoveWatcher overloads
   * that operate on a path or watch descriptor.
   *
   * @param it An iterator to the Watcher to be removed.
   * @returns 0 on success, -1 otherwise.
   */
  int RemoveWatcher(WatcherIterator it);

  /**
   * Look for a Watcher for the given path in the store.
   *
   * In order to check if the returned iterator is valid, callers must
   * use the WatcherNotFound method with the value returned from this
   * method.
   *
   * @param needle The path we want the Watcher for.
   * @returns An iterator to the Watcher.
   */
  WatcherIterator FindWatcher(const std::filesystem::path& needle);

  /**
   * Look for a Watcher for the given watch descriptor.
   *
   * Useful for searching for a Watcher from the inotify_event.wd
   * field when processing an event.
   *
   * In order to check if the returned iterator is valid, callers must
   * use the WatcherNotFound method with the value returned from this
   * method.
   *
   * @param needle The watch descriptor we want the Watcher for.
   * @returns An iterator to the Watcher.
   */
  WatcherIterator FindWatcher(int needle);

  /**
   * Validate if the given iterator is in the Watcher store.
   *
   * @param w An iterator, usually gotten by calling FindWatcher.
   * @returns true if the given iterator is not in the store.
   */
  bool WatcherNotFound(WatcherIterator w) { return w == watchers_.end(); }

  /**
   * Read events from the inotify subsystem.
   *
   * The events are read in bulk, but returned one at a time for the
   * caller to process. Successive calls will either return another
   * event that was read on a previous call or attempt to read more
   * events. If no event is read after some time, InotifyTimeout will
   * be returned instead, allowing the caller to do some additional
   * processing before calling GetNext again.
   *
   * @returns One of three variants:
   *          - inotify_event* when an event was successfully read.
   *          - InotifyError when a system error prevented reading events.
   *          - InotifyTimeout if everything is working as expected but
   *            no events were read after some time.
   */
  InotifyResult GetNext();

  /**
   * Translate an inotify_event->mask to a printable string.
   *
   * @returns A string in the format '<event_type> | <other_type>...'
   */
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

  // Iterable for event flags from <sys/inotify.h>
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
