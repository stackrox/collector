
#ifndef _STOPPABLE_THREAD_H_
#define _STOPPABLE_THREAD_H_

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>

#include "Logging.h"

namespace collector {

class StoppableThread {
 public:
  template <typename... Args>
  bool Start(Args&&... args) {
    if (!prepareStart()) return false;
    return doStart(new std::thread(std::forward<Args>(args)...));
  }
  void Stop();
  bool Pause(const std::chrono::system_clock::duration& duration) {
    return PauseUntil(std::chrono::system_clock::now() + duration);
  }
  bool PauseUntil(const std::chrono::system_clock::time_point& time_point);

  bool should_stop() const { return should_stop_.load(std::memory_order_relaxed); }
  int stop_fd() const { return stop_pipe_[0]; }

  bool running() const { return thread_ != nullptr; }

 private:
  bool prepareStart();
  bool doStart(std::thread* thread);

  std::unique_ptr<std::thread> thread_;
  std::mutex stop_mutex_;
  std::condition_variable stop_cond_;
  std::atomic<bool> should_stop_;
  int stop_pipe_[2];
};

}  // namespace collector

#endif  // _STOPPABLE_THREAD_H_
