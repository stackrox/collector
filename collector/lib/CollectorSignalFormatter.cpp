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

#include "CollectorSignalFormatter.h"
#include <google/protobuf/util/time_util.h>

#include <uuid/uuid.h>

#include "EventMap.h"
#include "Logging.h"
#include "Utility.h"

namespace collector {

using SignalStreamMessage = v1::SignalStreamMessage;
using Signal = CollectorSignalFormatter::Signal;
using ProcessSignal = CollectorSignalFormatter::ProcessSignal;
using ProcessCredentials = ProcessSignal::Credentials;

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

}

const SignalStreamMessage* CollectorSignalFormatter::ToProtoMessage(sinsp_evt* event) {
  Signal* signal;
  ProcessSignal* process_signal;

  if (process_signals[event->get_type()] == ProcessSignalType::UNKNOWN_PROCESS_TYPE) {
    return nullptr;
  }

  process_signal = CreateProcessSignal(event);
  if (!process_signal) return nullptr;
  signal = Allocate<Signal>();
  signal->set_allocated_process_signal(process_signal);

  SignalStreamMessage* signal_stream_message = AllocateRoot();
  signal_stream_message->clear_collector_register_request();
  signal_stream_message->set_allocated_signal(signal);
  return signal_stream_message;
}

const SignalStreamMessage* CollectorSignalFormatter::ToProtoMessage(sinsp_threadinfo* tinfo) {
  Signal* signal;
  ProcessSignal* process_signal;

  process_signal = CreateProcessSignal(tinfo);
  if (!process_signal) return nullptr;
  signal = Allocate<Signal>();
  signal->set_allocated_process_signal(process_signal);

  SignalStreamMessage* signal_stream_message = AllocateRoot();
  signal_stream_message->clear_collector_register_request();
  signal_stream_message->set_allocated_signal(signal);
  return signal_stream_message;
}


ProcessSignal* CollectorSignalFormatter::CreateProcessSignal(sinsp_evt* event) {
  auto signal = Allocate<ProcessSignal>();

  // set id
  signal->set_id(UUIDStr());
  // set name
  if (const char* name = event_extractor_.get_proc_name(event)) signal->set_name(name);
  // set process arguments
  if (const char* args = event_extractor_.get_proc_args(event)) signal->set_args(args);
  // set pid
  if (const int64_t* pid = event_extractor_.get_pid(event)) signal->set_pid(*pid);
  // set parent pid
  if (const int64_t* ppid = event_extractor_.get_ppid(event)) signal->set_parent_pid(*ppid);
  // set exec_file_path
  if (const std::string* exepath = event_extractor_.get_exepath(event)) signal->set_exec_file_path(*exepath);
  // set creds
  ProcessCredentials* process_creds = CreateProcessCreds(event);
  if (process_creds) signal->set_allocated_credentials(process_creds);
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

ProcessSignal* CollectorSignalFormatter::CreateProcessSignal(sinsp_threadinfo* tinfo) {
  auto signal = Allocate<ProcessSignal>();

  // set id
  signal->set_id(UUIDStr());
  // set name
  signal->set_name(tinfo->get_comm());
  // set process arguments
  if (tinfo->m_args.size() > 0) {
    std::ostringstream args;
    for (auto it = tinfo->m_args.begin(); it != tinfo->m_args.end();) {
      args << *it++;
      if (it != tinfo->m_args.end()) args << " ";
    }
    signal->set_args(args.str());
  }
  // set pid
  signal->set_pid(tinfo->m_pid);
  // set ppid
  signal->set_parent_pid(tinfo->m_ptid);
  // set exec_file_path
  signal->set_exec_file_path(tinfo->m_exepath);
  // set creds
  ProcessCredentials* process_creds = CreateProcessCreds(tinfo);
  if (process_creds) signal->set_allocated_credentials(process_creds);
  //set time
  auto timestamp = Allocate<Timestamp>();
  *timestamp = TimeUtil::NanosecondsToTimestamp(tinfo->m_clone_ts);
  signal->set_allocated_time(timestamp);
  // set container_id
  signal->set_container_id(tinfo->m_container_id);
  return signal;
}

ProcessCredentials* CollectorSignalFormatter::CreateProcessCreds(sinsp_evt* event) {
  const uint32_t* uid = event_extractor_.get_uid(event);
  const uint32_t* gid = event_extractor_.get_gid(event);

  if (!uid || !gid) return nullptr;
  if (*uid == 0xffffffff && *gid == 0xffffffff) return nullptr;

  auto creds = Allocate<ProcessCredentials>();
  if (uid) creds->set_uid(*uid);
  if (gid) creds->set_gid(*gid);
  return creds;
}

ProcessCredentials* CollectorSignalFormatter::CreateProcessCreds(sinsp_threadinfo* tinfo) {
  if (!tinfo) return nullptr;
  if (tinfo->m_uid == 0xffffffff || tinfo->m_gid == 0xffffffff) return nullptr;

  auto creds = Allocate<ProcessCredentials>();
  creds->set_uid(tinfo->m_uid);
  creds->set_gid(tinfo->m_gid);
  return creds;
}

}  // namespace collector
