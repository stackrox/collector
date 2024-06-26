#include "StoppableThread.h"

#include <iostream>
#include <unistd.h>

#include "Utility.h"

namespace collector {

bool StoppableThread::prepareStart() {
  if (running()) {
    CLOG(ERROR) << "Could not start thread: already running";
    return false;
  }
  should_stop_.store(false, std::memory_order_relaxed);
  if (pipe(stop_pipe_) != 0) {
    CLOG(ERROR) << "Could not create pipe for stop signals: " << StrError(errno);
    return false;
  }
  return true;
}

bool StoppableThread::doStart(std::thread* thread) {
  thread_.reset(thread);
  return true;
}

bool StoppableThread::PauseUntil(const std::chrono::system_clock::time_point& time_point) {
  std::unique_lock<std::mutex> lock(stop_mutex_);
  return !stop_cond_.wait_until(lock, time_point, [this]() { return should_stop(); });
}

void StoppableThread::Stop() {
  should_stop_.store(true, std::memory_order_relaxed);
  stop_cond_.notify_all();
  for (;;) {
    int rv = close(stop_pipe_[1]);
    if (rv != 0) {
      CLOG(ERROR) << "Failed to close writing end of pipe: " << StrError(errno);
      if (errno == EINTR) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        continue;
      }
    }
    break;
  }
  thread_->join();
  thread_.reset();
  int rv = close(stop_pipe_[0]);
  if (rv != 0) {
    CLOG(ERROR) << "Failed to close reading end of pipe: " << StrError(errno);
  }
}

}  // namespace collector
