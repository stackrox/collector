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

#include "ProcessSignalHandler.h"

#include <sstream>

#include "storage/process_indicator.pb.h"

#include "RateLimit.h"

namespace collector {

std::string compute_process_key(const ::storage::ProcessSignal& s) {
  std::stringstream ss;
  ss << s.container_id() << " " << s.name() << " ";
  if (s.args().length() <= 256) {
    ss << s.args();
  } else {
    ss.write(s.args().c_str(), 256);
  }
  ss << " " << s.exec_file_path();
  return ss.str();
}

bool ProcessSignalHandler::Start() {
  client_.Start();
  return true;
}

bool ProcessSignalHandler::Stop() {
  client_.Stop();
  rate_limiter_.ResetRateLimitCache();
  return true;
}

SignalHandler::Result ProcessSignalHandler::HandleSignal(sinsp_evt* evt) {
  const auto* signal_msg = formatter_.ToProtoMessage(evt);
  if (!signal_msg) {
    ++(stats_->nProcessResolutionFailuresByEvt);
    return IGNORED;
  }

  if (!rate_limiter_.Allow(compute_process_key(signal_msg->signal().process_signal()))) {
    ++(stats_->nProcessRateLimitCount);
    return IGNORED;
  }

  auto result = client_.PushSignals(*signal_msg);
  if (result == SignalHandler::PROCESSED) {
    ++(stats_->nProcessSent);
  } else if (result == SignalHandler::ERROR) {
    ++(stats_->nProcessSendFailures);
  }

  return result;
}

SignalHandler::Result ProcessSignalHandler::HandleExistingProcess(sinsp_threadinfo* tinfo) {
  const auto* signal_msg = formatter_.ToProtoMessage(tinfo);
  if (!signal_msg) {
    ++(stats_->nProcessResolutionFailuresByTinfo);
    return IGNORED;
  }

  if (!rate_limiter_.Allow(compute_process_key(signal_msg->signal().process_signal()))) {
    ++(stats_->nProcessRateLimitCount);
    return IGNORED;
  }

  auto result = client_.PushSignals(*signal_msg);
  if (result == SignalHandler::PROCESSED) {
    ++(stats_->nProcessSent);
  } else if (result == SignalHandler::ERROR) {
    ++(stats_->nProcessSendFailures);
  }

  return result;
}

std::vector<std::string> ProcessSignalHandler::GetRelevantEvents() {
  return {"execve<"};
}

}  // namespace collector
