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
