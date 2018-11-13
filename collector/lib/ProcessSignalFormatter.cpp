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

#include "ProcessSignalFormatter.h"
#include <google/protobuf/util/time_util.h>

#include <uuid/uuid.h>

#include "EventMap.h"
#include "Logging.h"
#include "Utility.h"

namespace collector {

using ProcessSignalMessage = sensor::ProcessSignalMessage;
using ProcessSignal = ProcessSignalFormatter::ProcessSignal;

using Timestamp = google::protobuf::Timestamp;
using TimeUtil = google::protobuf::util::TimeUtil;

namespace {

enum ProcessSignalType {
	EXECVE,
	UNKNOWN_PROCESS_TYPE
};

static EventMap<ProcessSignalType> process_signals = {
  {
    { "execve<", ProcessSignalType::EXECVE },
  },
  ProcessSignalType::UNKNOWN_PROCESS_TYPE,
};

string extract_proc_args(sinsp_threadinfo *tinfo) {
  if (tinfo->m_args.empty()) return "";
  std::ostringstream args;
  for (auto it = tinfo->m_args.begin(); it != tinfo->m_args.end();) {
    args << *it++;
    if (it != tinfo->m_args.end()) args << " ";
  }
  return args.str();
}

}

const ProcessSignalMessage* ProcessSignalFormatter::ToProtoMessage(sinsp_evt* event) {
  if (process_signals[event->get_type()] == ProcessSignalType::UNKNOWN_PROCESS_TYPE) {
    return nullptr;
  }

  if (!ValidateProcessDetails(event)) {
    CLOG(INFO) << "Dropping process event: invalid details";
    return nullptr;
  }

  Reset();
  ProcessSignal* process_signal = CreateProcessSignal(event);
  if (!process_signal) return nullptr;

  ProcessSignalMessage* process_message = AllocateRoot();
  process_message->clear_collector_register_request();
  process_message->set_allocated_process_signal(process_signal);
  return process_message;
}

const ProcessSignalMessage* ProcessSignalFormatter::ToProtoMessage(sinsp_threadinfo* tinfo) {
  if (!ValidateProcessDetails(tinfo)) {
    CLOG(INFO) << "Dropping process event: invalid details";
    return nullptr;
  }

  Reset();
  ProcessSignal* process_signal = CreateProcessSignal(tinfo);
  if (!process_signal) return nullptr;

  ProcessSignalMessage* process_message = AllocateRoot();
  process_message->clear_collector_register_request();
  process_message->set_allocated_process_signal(process_signal);
  return process_message;
}


ProcessSignal* ProcessSignalFormatter::CreateProcessSignal(sinsp_evt* event) {
  auto signal = Allocate<ProcessSignal>();

  // set id
  signal->set_id(UUIDStr());

  // set name
  if (const std::string* name = event_extractor_.get_comm(event)) signal->set_name(*name);

  // set process arguments
  if (const char* args = event_extractor_.get_proc_args(event)) signal->set_args(args);

  // set pid
  if (const int64_t* pid = event_extractor_.get_pid(event)) signal->set_pid(*pid);

  // set exec_file_path
  if (const std::string* exepath = event_extractor_.get_exepath(event)) signal->set_exec_file_path(*exepath);

  // set user and group id credentials
  if (const uint32_t* uid = event_extractor_.get_uid(event)) signal->set_uid(*uid);
  if (const uint32_t* gid = event_extractor_.get_gid(event)) signal->set_gid(*gid);

  //set time
  auto timestamp = Allocate<Timestamp>();
  *timestamp = TimeUtil::NanosecondsToTimestamp(event->get_ts());
  signal->set_allocated_time(timestamp);

  // set container_id
  if (const std::string* container_id = event_extractor_.get_container_id(event)) {
    signal->set_container_id(*container_id);
  }

  return signal;
}

ProcessSignal* ProcessSignalFormatter::CreateProcessSignal(sinsp_threadinfo* tinfo) {
  auto signal = Allocate<ProcessSignal>();

  // set id
  signal->set_id(UUIDStr());

  // set name
  signal->set_name(tinfo->get_comm());

  // set process arguments
  signal->set_args(extract_proc_args(tinfo));

  // set pid
  signal->set_pid(tinfo->m_pid);

  // set exec_file_path
  signal->set_exec_file_path(tinfo->m_exepath);

  // set user and group id credentials
  signal->set_uid(tinfo->m_uid);
  signal->set_gid(tinfo->m_gid);

  //set time
  auto timestamp = Allocate<Timestamp>();
  *timestamp = TimeUtil::NanosecondsToTimestamp(tinfo->m_clone_ts);
  signal->set_allocated_time(timestamp);

  // set container_id
  signal->set_container_id(tinfo->m_container_id);

  return signal;
}

bool ProcessSignalFormatter::ValidateProcessDetails(sinsp_threadinfo* tinfo) {
  if (tinfo->m_exepath == "<NA>") return false;
  if (tinfo->get_comm() == "<NA>") return false;

  return true;
}

bool ProcessSignalFormatter::ValidateProcessDetails(sinsp_evt* event) {
  const std::string* path = event_extractor_.get_exepath(event);
  if (path == nullptr || *path == "<NA>") return false;

  // missing exe path, replace with command
  const std::string* name = event_extractor_.get_comm(event);
  if (name == nullptr || *name == "<NA>") return false;

  return true;
}

}  // namespace collector
