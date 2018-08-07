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

#include <uuid/uuid.h>

#include "EventMap.h"
#include "Logging.h"

namespace collector {

using SignalStreamMessage = signal_service::SignalStreamMessage;
using Signal = CollectorSignalFormatter::Signal;
using ProcessSignal = CollectorSignalFormatter::ProcessSignal;
using ProcessCredentials = ProcessSignal::Credentials;

namespace {

enum SignalOp {
	EXECVE,
	UNKNOWN
};

static EventMap<SignalOp> signal_ops = {
  {
    { "execve<", SignalOp::EXECVE },
  },
  SignalOp::UNKNOWN,
};

}

const SignalStreamMessage* CollectorSignalFormatter::ToProtoMessage(sinsp_evt* event) {
  if (!signal_ops[event->get_type()]) {
    return nullptr;
  }

  SignalStreamMessage* signal_stream_message = AllocateRoot();

  Signal* signal = Allocate<Signal>();
  // set id
  uuid_t uuid;
  constexpr int kUuidStringLength = 36;  // uuid_unparse manpage says so. Feeling slightly uneasy still ...
  char uuid_str[kUuidStringLength + 1];
  uuid_generate_time_safe(uuid);
  uuid_unparse_lower(uuid, uuid_str);
  std::string id(uuid_str);
  signal->set_id(id);

  // set time
  signal->set_time_nanos(event->get_ts());

  // set container_id
  if (const std::string* container_id = event_extractor_.get_container_id(event)) {
    if (container_id->length() <= 12) {
      signal->set_container_id(*container_id);
    } else {
      signal->set_container_id(container_id->substr(0, 12));
    }
  }

  // set process info
  ProcessSignal* process_signal = CreateProcessSignal(event);

  signal->set_allocated_process_signal(process_signal);

  signal_stream_message->clear_collector_register_request();
  signal_stream_message->set_allocated_signal(signal);
  return signal_stream_message;
}

ProcessSignal* CollectorSignalFormatter::CreateProcessSignal(sinsp_evt* event) {
  auto signal = Allocate<ProcessSignal>();

  // set name
  if (const char* name = event_extractor_.get_proc_name(event)) signal->set_name(name);
  // set command_line
  if (const char* command_line = event_extractor_.get_exeline(event)) signal->set_command_line(command_line);
  // set pid
  if (const int64_t* pid = event_extractor_.get_pid(event)) signal->set_pid(*pid);
  // set exec_file_path
  if (const std::string* exe = event_extractor_.get_exe(event)) signal->set_exec_file_path(*exe);
  // set creds
  ProcessCredentials* process_creds = CreateProcessCreds(event);
  signal->set_allocated_credentials(process_creds);

  return signal;
}

ProcessCredentials* CollectorSignalFormatter::CreateProcessCreds(sinsp_evt* event) {
  auto creds = Allocate<ProcessCredentials>();

  if (const uint32_t* uid = event_extractor_.get_uid(event)) creds->set_uid(*uid);
  if (const uint32_t* gid = event_extractor_.get_gid(event)) creds->set_gid(*gid);
  // fill in remaining
  return creds;
}

}  // namespace collector
