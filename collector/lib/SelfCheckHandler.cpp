#include "SelfCheckHandler.h"

#include <array>
#include <chrono>

#include "Logging.h"
#include "SelfChecks.h"

namespace collector {

SignalHandler::Result SelfCheckProcessHandler::HandleSignal(sinsp_evt* evt) {
  if (isSelfCheckEvent(evt)) {
    CLOG(INFO) << "Found self-checks process event - driver appears to be working correctly";
    found_live_event_ = true;
    if (Finished()) {
      return FINISHED;
    }

    // trigger Existing Process handling, so we can look for the collector process.
    return NEEDS_REFRESH;
  } else {
    const std::string* name = event_extractor_.get_comm(evt);
    const std::string* exe = event_extractor_.get_exe(evt);

    if (name == nullptr || exe == nullptr) {
      CLOG(INFO) << "null name and exe";
    }

    if (name->compare(self_checks::kSelfChecksName) == 0 && exe->compare(self_checks::kSelfChecksExePath)) {
      CLOG(INFO) << "well aint that some bullshit";
    }
  }

  return IGNORED;
}

SignalHandler::Result SelfCheckProcessHandler::HandleExistingProcess(sinsp_threadinfo* tinfo) {
  auto exe = tinfo->get_exe();
  // CLOG(INFO) << "(SC) Existing process: " << exe;
  return IGNORED;
}

SignalHandler::Result SelfCheckNetworkHandler::HandleSignal(sinsp_evt* evt) {
  if (!isSelfCheckEvent(evt)) {
    return IGNORED;
  }

  const uint16_t* server_port = event_extractor_.get_server_port(evt);
  CLOG(INFO) << "Got self-checks with server port: " << (server_port ? *server_port : -1);
  return IGNORED;
}

SignalHandler::Result SelfCheckNetworkHandler::HandleExistingProcess(sinsp_threadinfo* tinfo) {
  return IGNORED;
}

}  // namespace collector
