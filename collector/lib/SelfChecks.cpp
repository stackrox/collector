#include "SelfChecks.h"

#include <cstdint>
#include <unistd.h>

#include <sys/wait.h>

#include "Logging.h"
#include "Utility.h"

namespace collector {

namespace self_checks {

const char* kSelfChecksExePath = "/usr/local/bin/self-checks";
const char* kSelfChecksName = "self-checks";
const uint16_t kSelfCheckServerPort = 1337;

int start_self_check_process() {
  pid_t child = fork();
  int status = 0;

  switch (child) {
    case -1:
      CLOG(FATAL) << "Failed to fork self-check process";
      break;
    case 0: {
      // in the child process
      std::string port_str = std::to_string(kSelfCheckServerPort);
      char* argv[] = {(char*)kSelfChecksExePath, (char*)port_str.c_str(), NULL};
      execve(kSelfChecksExePath, argv, NULL);

      // if execve fails for whatever reason, immediately exit
      // from this process
      exit(errno);
      break;
    }
    default:
      waitpid(child, &status, 0);
      CLOG(INFO) << "self-check (pid=" << child << ") exitted with status: " << WEXITSTATUS(status);
      if (status != 0) {
        CLOG(FATAL) << "self-checks failed to execute: " << StrError(WEXITSTATUS(status));
      }
      break;
  }

  return status;
}

}  // namespace self_checks

}  // namespace collector
