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
