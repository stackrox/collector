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

#include "ProcessSummaryFormatter.h"

#include <uuid/uuid.h>

#include "EventMap.h"
#include "Logging.h"

namespace collector {

namespace {

static EventMap<data::ProcessSummary_ProcessOp> process_ops = {
  {
    { "clone<", data::ProcessSummary::CLONE },
    { "execve<", data::ProcessSummary::EXECVE },
    { "procexit", data::ProcessSummary::EXIT },
    { "kill<", data::ProcessSummary::KILL },
    { "setns<", data::ProcessSummary::SETNS },
    { "setgid<", data::ProcessSummary::SETID },
    { "setuid<", data::ProcessSummary::SETID },
    { "delete_module>", data::ProcessSummary::MODULE },
    { "init_module>", data::ProcessSummary::MODULE },
    { "finit_module>", data::ProcessSummary::MODULE },
    { "capset<", data::ProcessSummary::CAP },
    { "bind<", data::ProcessSummary::SOCKET },
    { "listen<", data::ProcessSummary::SOCKET },
    { "chmod<", data::ProcessSummary::PERM },
    { "fchmod<", data::ProcessSummary::PERM },
    { "fchmodat<", data::ProcessSummary::PERM },
    { "chown<", data::ProcessSummary::PERM },
    { "fchown<", data::ProcessSummary::PERM },
    { "fchownat<", data::ProcessSummary::PERM },
    { "lchown<", data::ProcessSummary::PERM },
  },
  data::ProcessSummary::ProcessSummary::UNKNOWN,
};

}

using ProcessSummary = data::ProcessSummary;
using ProcessOperation = ProcessSummaryFormatter::ProcessOperation;
using ProcessOperationDetails = ProcessSummaryFormatter::ProcessOperationDetails;
using SocketDetails = ProcessSummaryFormatter::SocketDetails;
using ModuleDetails = ProcessSummaryFormatter::ModuleDetails;
using PermissionChangeDetails = ProcessSummaryFormatter::PermissionChangeDetails;
using ProcessContainer = ProcessSummaryFormatter::ProcessContainer;
using ProcessDetails = ProcessSummaryFormatter::ProcessDetails;

const ProcessSummary* ProcessSummaryFormatter::ToProtoMessage(sinsp_evt* event) {
  ProcessOp op = process_ops[event->get_type()];
  if (op == ProcessSummary::UNKNOWN) return nullptr;

  ProcessSummary* process_summary = AllocateRoot();
  ProcessOperation* operation = CreateProcessOperation(event, op);
  if (operation) {
    process_summary->mutable_ops()->AddAllocated(operation);
  }

  // Set process info
  ProcessDetails* process = CreateProcessDetails(event);
  if (process) {
    process_summary->set_allocated_process(process);
  }

  // Set container
  ProcessContainer* container = CreateProcessContainer(event);
  if (container) {
    process_summary->set_allocated_container(container);
  }

  // Set timestamp
  process_summary->set_timestamp(event->get_ts());

  // UUID is stored as raw bytes, not in string format.
  uuid_t uuid;
  uuid_generate(uuid);
  process_summary->mutable_id()->set_value(&uuid, sizeof(uuid));

  return process_summary;
}

ProcessContainer* ProcessSummaryFormatter::CreateProcessContainer(sinsp_evt* event) {
  const std::string* container_id = event_extractor_.get_container_id(event);
  if (!container_id) return nullptr;

  auto container = Allocate<ProcessContainer>();
  if (container_id->length() <= 12) {
    container->set_id(*container_id);
  } else {
    container->set_id(container_id->substr(0, 12));
  }

  if (const uint32_t* privileged = event_extractor_.get_container_privileged(event)) {
    container->set_privileged(*privileged != 0);
  }
  if (cluster_id_) {
    container->mutable_cluster_id()->set_value(*cluster_id_, sizeof(*cluster_id_));
  }

  return container;
}

ProcessDetails* ProcessSummaryFormatter::CreateProcessDetails(sinsp_evt* event) {
  auto details = Allocate<ProcessDetails>();

  if (const char* cwd = event_extractor_.get_cwd(event)) details->set_cwd(cwd);
  if (const std::string* exe = event_extractor_.get_exe(event)) details->set_exe(*exe);
  if (const char* exeline = event_extractor_.get_exeline(event)) details->set_exeline(exeline);
  if (const char* name = event_extractor_.get_proc_name(event)) details->set_name(name);
  if (const char* pname = event_extractor_.get_proc_pname(event)) details->set_parent_name(pname);
  if (const int64_t* pid = event_extractor_.get_pid(event)) details->set_pid(*pid);
  if (const char* user_name = event_extractor_.get_user_name(event)) details->set_user_name(user_name);
  if (const uint32_t* uid = event_extractor_.get_uid(event)) {
    details->set_uid(*uid);
  }

  return details;
}

ProcessOperation* ProcessSummaryFormatter::CreateProcessOperation(sinsp_evt* event, ProcessOp op) {
  auto operation = Allocate<data::ProcessSummary_ProcessOperation>();
  operation->set_syscall(event_names_.GetEventName(event->get_type()));
  operation->set_operation(op);
  if (const int64_t* result = event_extractor_.get_event_rawres(event)) {
    operation->set_failure(*result < 0);
  }

  data::ProcessSummary_ProcessOperation_Details* details = CreateProcessOperationDetails(event, op);
  if (details) {
    operation->set_allocated_details(details);
  }
  return operation;
}

SocketDetails* ProcessSummaryFormatter::CreateSocketDetails(sinsp_evt* event) {
  auto socket_details = Allocate<SocketDetails>();
  if (const uint16_t* port = event_extractor_.get_client_port(event)) {
    socket_details->set_client_port(*port);
  }
  if (const uint16_t* port = event_extractor_.get_server_port(event)) {
    socket_details->set_server_port(*port);
  }

  sinsp_fdinfo_t* fd_info = event->get_fd_info();
  if (fd_info) {
    switch (fd_info->m_type) {
      case SCAP_FD_IPV4_SOCK:
      case SCAP_FD_IPV4_SERVSOCK:
      case SCAP_FD_IPV6_SOCK:
      case SCAP_FD_IPV6_SERVSOCK:
        socket_details->set_family(ProcessSummary::IP);
        break;
      case SCAP_FD_UNIX_SOCK:
        socket_details->set_family(ProcessSummary::UNIX);
        break;
      default:
        break;
    }
  }
  return socket_details;
}

ModuleDetails* ProcessSummaryFormatter::CreateModuleDetails(sinsp_evt* event) {
  // delete_module has a name argument.
  const char* name = event_extractor_.get_evt_arg_name(event);

  if (!name) {
    // finit_module has an fd argument.
    const int64_t* fd = event_extractor_.get_evt_arg_fd(event);
    sinsp_threadinfo* tinfo = event->get_thread_info();
    if (fd && tinfo) {
      sinsp_fdinfo_t* fd_info = tinfo->get_fd(*fd);
      if (fd_info) {
        name = fd_info->m_name.c_str();
      }
    }
  }

  bool is_image = false;
  if (!name) {
    // init_module has a module_image argument.
    name = event_extractor_.get_evt_arg_module_image(event);
    if (name) is_image = true;
  }

  if (!name) {
    return nullptr;
  }

  auto module_details = Allocate<ModuleDetails>();
  if (is_image) {
    module_details->set_name(std::string("image:") + name);
  } else {
    module_details->set_name(name);
  }
  return module_details;
}

PermissionChangeDetails* ProcessSummaryFormatter::CreatePermissionChangeDetails(sinsp_evt* event) {
  const char* path = event_extractor_.get_evt_arg_pathname(event);
  if (!path) return nullptr;

  auto perm_details = Allocate<PermissionChangeDetails>();
  perm_details->set_path(path);
  return perm_details;
}

ProcessOperationDetails* ProcessSummaryFormatter::CreateProcessOperationDetails(sinsp_evt* event, ProcessOp op) {
  ProcessOperationDetails* details = nullptr;
  switch (op) {
    case ProcessSummary::SOCKET: {
      if (auto socket_details = CreateSocketDetails(event)) {
        details = Allocate<data::ProcessSummary_ProcessOperation_Details>();
        details->set_allocated_socket(socket_details);
      }
      break;
    }
    case ProcessSummary::MODULE: {
      if (auto module_details = CreateModuleDetails(event)) {
        details = Allocate<data::ProcessSummary_ProcessOperation_Details>();
        details->set_allocated_module(module_details);
      }
      break;
    }
    case ProcessSummary::PERM: {
      if (auto perm_details = CreatePermissionChangeDetails(event)) {
        details = Allocate<data::ProcessSummary_ProcessOperation_Details>();
        details->set_allocated_permissionchange(perm_details);
      }
      break;
    }
    default:
      break;
  }

  return details;
}

}  // namespace collector
