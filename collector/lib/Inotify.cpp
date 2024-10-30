#include "Inotify.h"

#include <filesystem>

namespace collector {

std::ostream& operator<<(std::ostream& os, const Watcher& w) {
  return os << "Watcher:"
            << "\n  path: " << w.path
            << "\n  wd: " << w.wd
            << "\n  tag: " << w.tag;
}

int Inotify::AddWatcher(const std::filesystem::path& p, uint32_t flags, int tag) {
  int wd = inotify_add_watch(fd_, p.c_str(), flags);
  if (wd < 0) {
    CLOG(ERROR) << "Unable to watch " << p << ": (" << errno << ") " << StrError();
    return -1;
  }

  auto it = FindWatcher(p);
  if (it == watchers_.end()) {
    watchers_.emplace_back(wd, p, tag);
  } else {
    if (it->wd != wd) {
      inotify_rm_watch(fd_, it->wd);
      it->wd = wd;
    }
    it->tag = tag;
  }

  return wd;
}

int Inotify::AddFileWatcher(const std::filesystem::path& p, int tag) {
  uint32_t flags = IN_MODIFY | IN_MOVE_SELF | IN_DELETE_SELF | IN_DONT_FOLLOW;
  CLOG(DEBUG) << "Adding file watcher to " << p;
  return AddWatcher(p, flags, tag);
}

int Inotify::AddDirectoryWatcher(const std::filesystem::path& p, int tag) {
  uint32_t flags = IN_CREATE | IN_DELETE | IN_MOVE_SELF | IN_DELETE_SELF | IN_DONT_FOLLOW;
  CLOG(DEBUG) << "Adding directory watcher to " << p;
  return AddWatcher(p, flags, tag);
}

int Inotify::RemoveWatcher(int wd) {
  return RemoveWatcher(FindWatcher(wd));
}

int Inotify::RemoveWatcher(const std::filesystem::path& p) {
  return RemoveWatcher(FindWatcher(p));
}

int Inotify::RemoveWatcher(WatcherIterator it) {
  int res = 0;
  if (it != watchers_.end()) {
    res = inotify_rm_watch(fd_, it->wd);
    watchers_.erase(it, it + 1);
  }
  return res;
}

Inotify::WatcherIterator Inotify::FindWatcher(const std::filesystem::path& needle) {
  for (auto it = watchers_.begin(); it != watchers_.end(); it++) {
    if (it->path == needle) {
      return it;
    }
  }
  return watchers_.end();
}

Inotify::WatcherIterator Inotify::FindWatcher(int needle) {
  for (auto it = watchers_.begin(); it != watchers_.end(); it++) {
    if (it->wd == needle) {
      return it;
    }
  }
  return watchers_.end();
}

InotifyResult Inotify::GetNext() {
  // Check if we have left over events from a previous call
  if (curr_byte_ < read_data_) {
    const auto* event = (const struct inotify_event*)&buffer_.at(curr_byte_);
    curr_byte_ += sizeof(struct inotify_event) + event->len;
    return event;
  }

  // Wait for new events
  fd_set rfds;
  struct timeval tv = {};

  tv.tv_sec = 2;
  tv.tv_usec = 0;
  FD_ZERO(&rfds);
  FD_SET(fd_, &rfds);

  int retval = select(fd_ + 1, &rfds, nullptr, nullptr, &tv);
  if (retval < 0) {
    return InotifyError("'select' call failed: (" + std::to_string(errno) + ") " + StrError());
  }

  if (retval == 0) {
    return InotifyTimeout();
  }

  // Received event from inotify.
  read_data_ = read(fd_, buffer_.data(), buffer_.size());
  if (read_data_ < 0) {
    return InotifyError("inotify: Unable to read event: (" + std::to_string(errno) + ") " + StrError());
  }

  const auto* event = (const struct inotify_event*)&buffer_.at(0);
  curr_byte_ = sizeof(struct inotify_event) + event->len;
  return event;
}

std::string Inotify::MaskToString(uint32_t mask) {
  std::string out;
  for (size_t i = 0; i < BIT_MASKS.size(); i++) {
    if ((mask & BIT_MASKS.at(i)) != 0) {
      if (!out.empty()) {
        out += " | ";
      }
      out += MASK_STR.at(i);
    }
  }

  return out;
}
}  // namespace collector
