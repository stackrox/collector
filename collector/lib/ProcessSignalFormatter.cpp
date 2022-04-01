#include "ProcessSignalFormatter.h"

#include <uuid/uuid.h>

#include <google/protobuf/util/time_util.h>

#include "internalapi/sensor/signal_iservice.pb.h"

#include "CollectorStats.h"
#include "EventMap.h"
#include "Logging.h"
#include "Utility.h"

namespace collector {

using SignalStreamMessage = sensor::SignalStreamMessage;
using Signal = ProcessSignalFormatter::Signal;
using ProcessSignal = ProcessSignalFormatter::ProcessSignal;
using LineageInfo = ProcessSignalFormatter::LineageInfo;

using Timestamp = google::protobuf::Timestamp;
using TimeUtil = google::protobuf::util::TimeUtil;

namespace {

enum ProcessSignalType {
  EXECVE,
  UNKNOWN_PROCESS_TYPE
};

static EventMap<ProcessSignalType> process_signals = {
    {
        {"execve<", ProcessSignalType::EXECVE},
    },
    ProcessSignalType::UNKNOWN_PROCESS_TYPE,
};

string extract_proc_args(sinsp_threadinfo* tinfo) {
  if (tinfo->m_args.empty()) return "";
  std::ostringstream args;
  for (auto it = tinfo->m_args.begin(); it != tinfo->m_args.end();) {
    args << *it++;
    if (it != tinfo->m_args.end()) args << " ";
  }
  return args.str();
}

}  // namespace

const SignalStreamMessage* ProcessSignalFormatter::ToProtoMessage(sinsp_evt* event) {
  if (process_signals[event->get_type()] == ProcessSignalType::UNKNOWN_PROCESS_TYPE) {
    return nullptr;
  }

  Reset();

  if (!ValidateProcessDetails(event)) {
    CLOG(INFO) << "Dropping process event: " << ProcessDetails(event);
    return nullptr;
  }

  ProcessSignal* process_signal = CreateProcessSignal(event);
  if (!process_signal) return nullptr;

  Signal* signal = Allocate<Signal>();
  signal->set_allocated_process_signal(process_signal);

  SignalStreamMessage* signal_stream_message = AllocateRoot();
  signal_stream_message->clear_collector_register_request();
  signal_stream_message->set_allocated_signal(signal);
  return signal_stream_message;
}

const SignalStreamMessage* ProcessSignalFormatter::ToProtoMessage(sinsp_threadinfo* tinfo) {
  Reset();
  if (!ValidateProcessDetails(tinfo)) {
    CLOG(INFO) << "Dropping process event: " << tinfo;
    return nullptr;
  }

  ProcessSignal* process_signal = CreateProcessSignal(tinfo);
  if (!process_signal) return nullptr;

  Signal* signal = Allocate<Signal>();
  signal->set_allocated_process_signal(process_signal);

  SignalStreamMessage* signal_stream_message = AllocateRoot();
  signal_stream_message->clear_collector_register_request();
  signal_stream_message->set_allocated_signal(signal);
  return signal_stream_message;
}

ProcessSignal* ProcessSignalFormatter::CreateProcessSignal(sinsp_evt* event) {
  auto signal = Allocate<ProcessSignal>();

  // set id
  signal->set_id(UUIDStr());

  const std::string* name = event_extractor_.get_comm(event);
  const std::string* exepath = event_extractor_.get_exepath(event);

  // set name (if name is missing or empty, try to use exec_file_path)
  if (name && !name->empty() && *name != "<NA>") {
    signal->set_name(*name);
  } else if (exepath && !exepath->empty() && *exepath != "<NA>") {
    signal->set_name(*exepath);
  }

  // set exec_file_path (if exec_file_path is missing or empty, try to use name)
  if (exepath && !exepath->empty() && *exepath != "<NA>") {
    signal->set_exec_file_path(*exepath);
  } else if (name && !name->empty() && *name != "<NA>") {
    signal->set_exec_file_path(*name);
  }

  // set process arguments
  if (const char* args = event_extractor_.get_proc_args(event)) signal->set_args(args);

  // set pid
  if (const int64_t* pid = event_extractor_.get_pid(event)) signal->set_pid(*pid);

  // set user and group id credentials
  if (const uint32_t* uid = event_extractor_.get_uid(event)) signal->set_uid(*uid);
  if (const uint32_t* gid = event_extractor_.get_gid(event)) signal->set_gid(*gid);

  // set time
  auto timestamp = Allocate<Timestamp>();
  *timestamp = TimeUtil::NanosecondsToTimestamp(event->get_ts());
  signal->set_allocated_time(timestamp);

  // set container_id
  if (const std::string* container_id = event_extractor_.get_container_id(event)) {
    signal->set_container_id(*container_id);
  }

  // set process lineage
  std::vector<LineageInfo> lineage;
  this->GetProcessLineage(event->get_thread_info(), lineage);
  for (const auto& p : lineage) {
    auto signal_lineage = signal->add_lineage_info();
    signal_lineage->set_parent_exec_file_path(p.parent_exec_file_path());
    signal_lineage->set_parent_uid(p.parent_uid());
  }

  return signal;
}

ProcessSignal* ProcessSignalFormatter::CreateProcessSignal(sinsp_threadinfo* tinfo) {
  auto signal = Allocate<ProcessSignal>();

  // set id
  signal->set_id(UUIDStr());

  auto name = tinfo->get_comm();
  auto exepath = tinfo->m_exepath;

  // set name (if name is missing or empty, try to use exec_file_path)
  if (!name.empty() && name != "<NA>") {
    signal->set_name(name);
  } else if (!exepath.empty() && exepath != "<NA>") {
    signal->set_name(exepath);
  }

  // set exec_file_path (if exec_file_path is missing or empty, try to use name)
  if (!exepath.empty() && exepath != "<NA>") {
    signal->set_exec_file_path(exepath);
  } else if (!name.empty() && name != "<NA>") {
    signal->set_exec_file_path(name);
  }

  // set the process as coming from a scrape as opposed to an exec
  signal->set_scraped(true);

  // set process arguments
  signal->set_args(extract_proc_args(tinfo));

  // set pid
  signal->set_pid(tinfo->m_pid);

  // set user and group id credentials
  signal->set_uid(tinfo->m_uid);
  signal->set_gid(tinfo->m_gid);

  // set time
  auto timestamp = Allocate<Timestamp>();
  *timestamp = TimeUtil::NanosecondsToTimestamp(tinfo->m_clone_ts);
  signal->set_allocated_time(timestamp);

  // set container_id
  signal->set_container_id(tinfo->m_container_id);

  // set process lineage
  std::vector<LineageInfo> lineage;
  GetProcessLineage(tinfo, lineage);

  for (const auto& p : lineage) {
    auto signal_lineage = signal->add_lineage_info();
    signal_lineage->set_parent_exec_file_path(p.parent_exec_file_path());
    signal_lineage->set_parent_uid(p.parent_uid());
  }

  return signal;
}

bool ProcessSignalFormatter::ValidateProcessDetails(sinsp_threadinfo* tinfo) {
  if (tinfo->m_exepath == "<NA>" && tinfo->get_comm() == "<NA>") {
    return false;
  }

  return true;
}

std::string ProcessSignalFormatter::ProcessDetails(sinsp_evt* event) {
  std::stringstream ss;
  const std::string* path = event_extractor_.get_exepath(event);
  const std::string* name = event_extractor_.get_comm(event);
  const std::string* container_id = event_extractor_.get_container_id(event);
  const char* args = event_extractor_.get_proc_args(event);
  const int64_t* pid = event_extractor_.get_pid(event);

  ss << "Container: " << (container_id ? *container_id : "null")
     << ", Name: " << (name ? *name : "null")
     << ", PID: " << (pid ? *pid : -1)
     << ", Path: " << (path ? *path : "null")
     << ", Args: " << (args ? args : "null");

  return ss.str();
}

bool ProcessSignalFormatter::ValidateProcessDetails(sinsp_evt* event) {
  const std::string* path = event_extractor_.get_exepath(event);
  const std::string* name = event_extractor_.get_comm(event);

  if ((path == nullptr || *path == "<NA>") && (name == nullptr || *name == "<NA>")) {
    return false;
  }

  return true;
}

int ProcessSignalFormatter::GetTotalStringLength(const std::vector<LineageInfo>& lineage) {
  int totalStringLength = 0;
  for (LineageInfo l : lineage) totalStringLength += l.parent_exec_file_path().size();

  return totalStringLength;
}

void ProcessSignalFormatter::CountLineage(const std::vector<LineageInfo>& lineage) {
  int totalStringLength = GetTotalStringLength(lineage);
  COUNTER_INC(CollectorStats::process_lineage_counts);
  COUNTER_ADD(CollectorStats::process_lineage_total, lineage.size());
  COUNTER_ADD(CollectorStats::process_lineage_sqr_total, lineage.size() * lineage.size());
  COUNTER_ADD(CollectorStats::process_lineage_string_total, totalStringLength);
}

void ProcessSignalFormatter::GetProcessLineage(sinsp_threadinfo* tinfo,
                                               std::vector<LineageInfo>& lineage) {
  if (tinfo == NULL) return;
  sinsp_threadinfo* mt = NULL;
  if (tinfo->is_main_thread()) {
    mt = tinfo;
  } else {
    mt = tinfo->get_main_thread();
    if (mt == NULL) return;
  }
  sinsp_threadinfo::visitor_func_t visitor = [this, &lineage](sinsp_threadinfo* pt) {
    if (pt == NULL) return false;
    if (pt->m_pid == 0) return false;

    //
    // Collection of process lineage information should stop at the container
    // boundary to avoid collecting host process information.
    //
    // In back-ported eBPF probes, `m_vpid` will not be set for containers
    // running when collector comes online because /proc/{pid}/status does
    // not contain namespace information, so `m_container_id` is checked
    // instead. `m_container_id` is not enough on its own to identify
    // containerized processes, because it is not guaranteed to be set on
    // all platforms.
    //
    if (pt->m_vpid == 0) {
      if (pt->m_container_id.empty()) {
        return false;
      }
    } else if (pt->m_pid == pt->m_vpid) {
      return false;
    }

    if (pt->m_vpid == -1) return false;

    // Collapse parent child processes that have the same path
    if (lineage.empty() || (lineage.back().parent_exec_file_path() != pt->m_exepath)) {
      LineageInfo info;
      info.set_parent_uid(pt->m_uid);
      info.set_parent_exec_file_path(pt->m_exepath);
      lineage.push_back(info);
    }

    // Limit max number of ancestors
    if (lineage.size() >= 10) return false;

    return true;
  };
  mt->traverse_parent_state(visitor);
  CountLineage(lineage);
}

}  // namespace collector
