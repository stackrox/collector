#include "Formatter.h"

#include <uuid/uuid.h>

#include <google/protobuf/util/time_util.h>

#include "internalapi/sensor/collector.pb.h"
#include "internalapi/sensor/signal_iservice.pb.h"

#include "CollectorStats.h"
#include "EventMap.h"
#include "Logging.h"
#include "Utility.h"
#include "system-inspector/EventExtractor.h"

namespace collector::output {

using SignalStreamMessage = sensor::SignalStreamMessage;
using Signal = Formatter::Signal;
using ProcessSignal = Formatter::ProcessSignal;
using LineageInfo = Formatter::LineageInfo;

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

std::string extract_proc_args(sinsp_threadinfo* tinfo) {
  if (tinfo->m_args.empty()) {
    return "";
  }
  std::ostringstream args;
  for (auto it = tinfo->m_args.begin(); it != tinfo->m_args.end();) {
    auto arg = *it++;
    auto arg_sanitized = SanitizedUTF8(arg);

    args << ((arg_sanitized ? *arg_sanitized : arg));

    if (it != tinfo->m_args.end()) {
      args << " ";
    }
  }
  return args.str();
}

}  // namespace

Formatter::Formatter(sinsp* inspector, const CollectorConfig& config)
    : event_names_(EventNames::GetInstance()),
      event_extractor_(std::make_unique<system_inspector::EventExtractor>()),
      container_metadata_(inspector),
      config_(config) {
  event_extractor_->Init(inspector);
}

Formatter::~Formatter() = default;

const sensor::ProcessSignal* Formatter::ToProtoMessage(sinsp_evt* event) {
  if (process_signals[event->get_type()] == ProcessSignalType::UNKNOWN_PROCESS_TYPE) {
    return nullptr;
  }

  Reset();

  if (!ValidateProcessDetails(event)) {
    CLOG(INFO) << "Dropping process event: " << ToString(event);
    return nullptr;
  }

  return CreateProcessSignal(event);
}

const sensor::ProcessSignal* Formatter::ToProtoMessage(sinsp_threadinfo* tinfo) {
  Reset();
  if (!ValidateProcessDetails(tinfo)) {
    CLOG(INFO) << "Dropping process event: " << tinfo;
    return nullptr;
  }

  return CreateProcessSignal(tinfo);
}

ProcessSignal* Formatter::CreateProcessSignal(sinsp_evt* event) {
  auto signal = AllocateRoot();

  // set id
  signal->set_id(UUIDStr());

  const std::string* name = event_extractor_->get_comm(event);
  const std::string* exepath = event_extractor_->get_exepath(event);

  std::optional<std::string> name_sanitized;
  std::optional<std::string> exepath_sanitized;

  if (name) {
    name_sanitized = SanitizedUTF8(*name);
  }

  if (exepath) {
    exepath_sanitized = SanitizedUTF8(*exepath);
  }

  // set name (if name is missing or empty, try to use exec_file_path)
  if (name && !name->empty() && *name != "<NA>") {
    signal->set_name(name_sanitized ? *name_sanitized : *name);
  } else if (exepath && !exepath->empty() && *exepath != "<NA>") {
    signal->set_name(exepath_sanitized ? *exepath_sanitized : *exepath);
  }

  // set exec_file_path (if exec_file_path is missing or empty, try to use name)
  if (exepath && !exepath->empty() && *exepath != "<NA>") {
    signal->set_exec_file_path(exepath_sanitized ? *exepath_sanitized : *exepath);
  } else if (name && !name->empty() && *name != "<NA>") {
    signal->set_exec_file_path(name_sanitized ? *name_sanitized : *name);
  }

  // set process arguments, if not explicitely disabled
  if (!config_.DisableProcessArguments()) {
    if (const char* args = event_extractor_->get_proc_args(event)) {
      std::string args_str = args;
      auto args_sanitized = SanitizedUTF8(args_str);
      signal->set_args(args_sanitized ? *args_sanitized : args_str);
    }
  }

  // set pid
  if (const int64_t* pid = event_extractor_->get_pid(event)) {
    signal->set_pid(*pid);
  }

  // set user and group id credentials
  if (const uint32_t* uid = event_extractor_->get_uid(event)) {
    signal->set_uid(*uid);
  }
  if (const uint32_t* gid = event_extractor_->get_gid(event)) {
    signal->set_gid(*gid);
  }

  // set time
  auto timestamp = Allocate<Timestamp>();
  *timestamp = TimeUtil::NanosecondsToTimestamp(event->get_ts());
  signal->set_allocated_creation_time(timestamp);

  // set container_id
  if (const std::string* container_id = event_extractor_->get_container_id(event)) {
    signal->set_container_id(*container_id);
  }

  // set process lineage
  auto lineage = GetProcessLineage(event->get_thread_info());
  for (const auto& p : lineage) {
    auto* signal_lineage = signal->add_lineage_info();
    signal_lineage->set_parent_exec_file_path(p.parent_exec_file_path());
    signal_lineage->set_parent_uid(p.parent_uid());
  }

  CLOG(DEBUG) << "Process (" << signal->container_id() << ": " << signal->pid() << "): "
              << signal->name() << "[" << container_metadata_.GetNamespace(event) << "] "
              << " (" << signal->exec_file_path() << ")"
              << " " << signal->args();

  return signal;
}

ProcessSignal* Formatter::CreateProcessSignal(sinsp_threadinfo* tinfo) {
  auto signal = AllocateRoot();

  // set id
  signal->set_id(UUIDStr());

  const auto& name = tinfo->m_comm;
  auto name_sanitized = SanitizedUTF8(name);
  const auto& exepath = tinfo->m_exepath;
  auto exepath_sanitized = SanitizedUTF8(exepath);

  // set name (if name is missing or empty, try to use exec_file_path)
  if (!name.empty() && name != "<NA>") {
    signal->set_name(name_sanitized ? *name_sanitized : name);
  } else if (!exepath.empty() && exepath != "<NA>") {
    signal->set_name(exepath_sanitized ? *exepath_sanitized : exepath);
  }

  // set exec_file_path (if exec_file_path is missing or empty, try to use name)
  if (!exepath.empty() && exepath != "<NA>") {
    signal->set_exec_file_path(exepath_sanitized ? *exepath_sanitized : exepath);
  } else if (!name.empty() && name != "<NA>") {
    signal->set_exec_file_path(name_sanitized ? *name_sanitized : name);
  }

  // set the process as coming from a scrape as opposed to an exec
  signal->set_scraped(true);

  // set process arguments
  signal->set_args(extract_proc_args(tinfo));

  // set pid
  signal->set_pid(tinfo->m_pid);

  // set user and group id credentials
  signal->set_uid(tinfo->m_user.uid());
  signal->set_gid(tinfo->m_group.gid());

  // set time
  auto timestamp = Allocate<Timestamp>();
  *timestamp = TimeUtil::NanosecondsToTimestamp(tinfo->m_clone_ts);
  signal->set_allocated_creation_time(timestamp);

  // set container_id
  signal->set_container_id(tinfo->m_container_id);

  // set process lineage
  auto lineage = GetProcessLineage(tinfo);

  for (const auto& p : lineage) {
    auto* signal_lineage = signal->add_lineage_info();
    signal_lineage->set_parent_exec_file_path(p.parent_exec_file_path());
    signal_lineage->set_parent_uid(p.parent_uid());
  }

  CLOG(DEBUG) << "Process (" << signal->container_id() << ": " << signal->pid() << "): "
              << signal->name()
              << " (" << signal->exec_file_path() << ")"
              << " " << signal->args();

  return signal;
}

std::string Formatter::ToString(sinsp_evt* event) {
  std::stringstream ss;
  const std::string* path = event_extractor_->get_exepath(event);
  const std::string* name = event_extractor_->get_comm(event);
  const std::string* container_id = event_extractor_->get_container_id(event);
  const char* args = event_extractor_->get_proc_args(event);
  const int64_t* pid = event_extractor_->get_pid(event);

  ss << "Container: " << (container_id ? *container_id : "null")
     << ", Name: " << (name ? *name : "null")
     << ", PID: " << (pid ? *pid : -1)
     << ", Path: " << (path ? *path : "null")
     << ", Args: " << (args ? args : "null");

  return ss.str();
}

bool Formatter::ValidateProcessDetails(const sinsp_threadinfo* tinfo) {
  if (tinfo == nullptr) {
    return false;
  }

  if (tinfo->m_exepath == "<NA>" && tinfo->m_comm == "<NA>") {
    return false;
  }

  return true;
}

bool Formatter::ValidateProcessDetails(sinsp_evt* event) {
  return ValidateProcessDetails(event->get_thread_info());
}

void Formatter::UpdateLineageStats(const std::vector<LineageInfo>& lineage) {
  int string_total = std::accumulate(lineage.cbegin(), lineage.cend(), 0, [](int acc, const auto& l) {
    return acc + l.parent_exec_file_path().size();
  });

  COUNTER_INC(CollectorStats::process_lineage_counts);
  COUNTER_ADD(CollectorStats::process_lineage_total, lineage.size());
  COUNTER_ADD(CollectorStats::process_lineage_sqr_total, lineage.size() * lineage.size());
  COUNTER_ADD(CollectorStats::process_lineage_string_total, string_total);
}

std::vector<LineageInfo> Formatter::GetProcessLineage(sinsp_threadinfo* tinfo) {
  std::vector<LineageInfo> lineage;
  if (tinfo == nullptr) {
    return lineage;
  }

  sinsp_threadinfo* mt = tinfo->get_main_thread();
  if (mt == nullptr) {
    return lineage;
  }

  sinsp_threadinfo::visitor_func_t visitor = [&lineage](sinsp_threadinfo* pt) {
    if (pt == nullptr) {
      return false;
    }
    if (pt->m_pid == 0) {
      return false;
    }

    // Collection of process lineage information should stop at the container
    // boundary to avoid collecting host process information.
    if (pt->m_pid == pt->m_vpid) {
      return false;
    }

    if (pt->m_vpid == -1) {
      return false;
    }

    // Collapse parent child processes that have the same path
    if (lineage.empty() || (lineage.back().parent_exec_file_path() != pt->m_exepath)) {
      LineageInfo info;
      info.set_parent_uid(pt->m_user.uid());
      info.set_parent_exec_file_path(pt->m_exepath);
      lineage.push_back(info);
    }

    // Limit max number of ancestors
    if (lineage.size() >= 10) {
      return false;
    }

    return true;
  };
  mt->traverse_parent_state(visitor);
  UpdateLineageStats(lineage);

  return lineage;
}

}  // namespace collector::output
