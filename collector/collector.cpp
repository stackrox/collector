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

#include <atomic>
#include <fstream>
#include <iostream>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <cctype>
#include <string>
#include <sys/wait.h>
#include <thread>

extern "C" {
#include <assert.h>
#include <execinfo.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/resource.h>

#include <cap-ng.h>
}

#include "CollectorArgs.h"
#include "CollectorService.h"
#include "EventNames.h"
#include "GetStatus.h"
#include "GRPC.h"
#include "LogLevel.h"
#include "Logging.h"
#include "SysdigService.h"
#include "CollectorStatsExporter.h"
#include "Utility.h"

#define finit_module(fd, opts, flags) syscall(__NR_finit_module, fd, opts, flags)
#define delete_module(name, flags) syscall(__NR_delete_module, name, flags)

using namespace collector;

static std::atomic<CollectorService::ControlValue> g_control(CollectorService::RUN);
static std::atomic<int> g_signum(0);

static void ShutdownHandler(int signum) {
  // Only set the control variable; the collector service will take care of the rest.
  g_signum.store(signum);
  g_control.store(CollectorService::STOP_COLLECTOR);
}

static void AbortHandler(int signum) {
  // Write a stacktrace to stderr
  void* buffer[32];
  size_t size = backtrace(buffer, 32);
  backtrace_symbols_fd(buffer, size, STDERR_FILENO);

  // Write a message to stderr using only reentrant functions.
  char message_buffer[256];
  int num_bytes = snprintf(message_buffer, sizeof(message_buffer), "Caught signal %d (%s): %s\n",
                           signum, SignalName(signum), strsignal(signum));
  write(STDERR_FILENO, message_buffer, num_bytes);

  // Re-raise the signal (this time routing it to the default handler) to make sure we get the correct exit code.
  signal(signum, SIG_DFL);
  raise(signum);
}

std::string GetHostPath(const std::string& file) {
    const char* host_root = std::getenv("SYSDIG_HOST_ROOT");
    if (!host_root) host_root = "";
    std::string host_file(host_root);
    host_file += file;
    return host_file;
}

int InsertModule(int fd, const std::unordered_map<std::string, std::string>& args) {
  std::string args_str;
  bool first = true;
  for (auto& entry : args) {
    if (first) first = false;
    else args_str += " ";
    args_str += entry.first + "=" + entry.second;
  }
  CLOG(DEBUG) << "Kernel module arguments: " << args_str;
  int res = finit_module(fd, args_str.c_str(), 0);
  if (res != 0) return res;
  struct stat st;
  std::string param_dir = GetHostPath(std::string("/sys/module/") + SysdigService::kModuleName + "/parameters/");
  res = stat(param_dir.c_str(), &st);
  if (res != 0) {
    // This is not optimal, but don't fail hard on systems where for whatever reason the above directory does not exist.
    CLOG(WARNING) << "Could not stat " << param_dir << ": " << StrError()
                  << ". No parameter verification can be performed.";
    return 0;
  }
  for (const auto& entry : args) {
    std::string param_file = param_dir + entry.first;
    res = stat(param_file.c_str(), &st);
    if (res != 0) {
      CLOG(ERROR) << "Could not stat " << param_file << ": " << StrError() << ". Parameter " << entry.first
                  << " is unsupported, suspecting module version mismatch.";
      errno = EINVAL;
      return -1;
    }
  }
  return 0;
}

// insertModule
// Method to insert the kernel module. The options to the module are computed
// from the collector configuration. Specifically, the syscalls that we should
// extract
void insertModule(const Json::Value& syscall_list) {
    std::unordered_map<std::string, std::string> module_args;

    std::string& syscall_ids = module_args["s_syscallIds"];
    // Iterate over the syscalls that we want and pull each of their ids.
    // These are stashed into a string that will get passed to init_module
    // to insert the kernel module
    const EventNames& event_names = EventNames::GetInstance();
    if (!syscall_list.isArray()) {
      CLOG(FATAL) << "Syscall list JSON is not an array: " << syscall_list.toStyledString();
    }
    for (const auto& syscall_json : syscall_list) {
        for (ppm_event_type id : event_names.GetEventIDs(syscall_json.asString())) {
            syscall_ids += std::to_string(id) + ",";
        }
    }
    syscall_ids += "-1";

    module_args["exclude_initns"] = "1";
    module_args["exclude_selfns"] = "1";
    module_args["verbose"] = "0";

    int fd = open(SysdigService::kModulePath, O_RDONLY);
    if (fd < 0) {
        CLOG(FATAL) << "Cannot open kernel module: " << SysdigService::kModulePath << ". Aborting...";
    }

    CLOG(INFO) << "Inserting kernel module " << SysdigService::kModulePath
               << " with indefinite removal and retry if required.";

    // Attempt to insert the module. If it is already inserted then remove it and
    // try again
    int result = InsertModule(fd, module_args);
    while (result != 0) {
        if (errno == EEXIST) {
            // note that we forcefully remove the kernel module whether or not it has a non-zero
            // reference count. There is only one container that is ever expected to be using
            // this kernel module and that is us
            delete_module(SysdigService::kModuleName, O_NONBLOCK | O_TRUNC);
            sleep(2);    // wait for 2s before trying again
        } else {
            CLOG(FATAL) << "Error inserting kernel module: " << SysdigService::kModulePath  << ": " << StrError()
                        << ". Aborting...";
        }
        result = InsertModule(fd, module_args);
    }
    close(fd);

    CLOG(INFO) << "Done inserting kernel module " << SysdigService::kModulePath << ".";
}

static const std::string base64_chars =
             "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
             "abcdefghijklmnopqrstuvwxyz"
             "0123456789+/";

static inline bool is_base64(unsigned char c) {
    return (std::isalnum(c) || (c == '+') || (c == '/'));
}

std::string base64_decode(std::string const& encoded_string) {
    int in_len = encoded_string.size();
    int i = 0;
    int j = 0;
    int in_ = 0;
    unsigned char char_array_4[4], char_array_3[3];
    std::string ret;

    while (in_len-- && ( encoded_string[in_] != '=') && is_base64(encoded_string[in_])) {
        char_array_4[i++] = encoded_string[in_]; in_++;
        if (i ==4) {
            for (i = 0; i <4; i++)
                char_array_4[i] = base64_chars.find(char_array_4[i]);

            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

            for (i = 0; (i < 3); i++)
                ret += char_array_3[i];
            i = 0;
        }
    }

    if (i) {
        for (j = i; j <4; j++)
            char_array_4[j] = 0;

        for (j = 0; j <4; j++)
            char_array_4[j] = base64_chars.find(char_array_4[j]);

        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
        char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

        for (j = 0; (j < i - 1); j++) ret += char_array_3[j];
    }

    return ret;
}

const char* GetHostname() {
  const char* hostname = std::getenv("NODE_HOSTNAME");
  if (hostname && *hostname) return hostname;

  CLOG(ERROR) << "Failed to determine hostname, environment variable NODE_HOSTNAME not set";
  return "unknown";
}

int main(int argc, char **argv) {
  // First action: drop all capabilities except for SYS_MODULE (inserting the module), SYS_PTRACE (reading from /proc),
  // and DAC_OVERRIDE (opening the device files with O_RDWR regardless of actual permissions).
  capng_clear(CAPNG_SELECT_BOTH);
  capng_updatev(CAPNG_ADD, static_cast<capng_type_t>(CAPNG_EFFECTIVE | CAPNG_PERMITTED),
                CAP_SYS_MODULE, CAP_DAC_OVERRIDE, CAP_SYS_PTRACE, -1);
  if (capng_apply(CAPNG_SELECT_BOTH) != 0) {
    CLOG(WARNING) << "Failed to drop capabilities: " << StrError();
  }

  if (!g_control.is_lock_free()) {
    CLOG(FATAL) << "Could not create a lock-free control variable!";
  }

  CollectorArgs *args = CollectorArgs::getInstance();
  int exitCode = 0;
  if (!args->parse(argc, argv, exitCode)) {
      if (!args->Message().empty()) {
          CLOG(FATAL) << args->Message();
      }
      CLOG(FATAL) << "Error parsing arguments";
  }

#ifdef COLLECTOR_CORE
  struct rlimit limit;
  limit.rlim_cur = RLIM_INFINITY;
  limit.rlim_max = RLIM_INFINITY;
  if (setrlimit(RLIMIT_CORE, &limit) != 0) {
    CLOG(ERROR) << "setrlimit() failed: " << StrError();
  }
#endif

  // insert the kernel module with options from the configuration
  Json::Value collectorConfig = args->CollectorConfig();

  insertModule(collectorConfig["syscalls"]);

  // Drop SYS_MODULE capability after successfully inserting module.
  capng_updatev(CAPNG_DROP, static_cast<capng_type_t>(CAPNG_EFFECTIVE | CAPNG_PERMITTED), CAP_SYS_MODULE, -1);
  if (capng_apply(CAPNG_SELECT_BOTH) != 0) {
    CLOG(WARNING) << "Failed to drop SYS_MODULE capability: " << StrError();
  }

  bool useGRPC = false;
  if (!args->GRPCServer().empty()) {
    useGRPC = true;
  }

  if (useGRPC) {
    grpc_init();
  } else {
    CLOG(INFO) << "GRPC is disabled. Specify GRPC_SERVER='server addr' env and signalFormat = 'signal_summary' and  signalOutput = 'grpc'";
  }

  std::string chiselB64 = args->Chisel();
  std::string chisel = base64_decode(chiselB64);

  // Extract configuration options
  bool useChiselCache = collectorConfig["useChiselCache"].asBool();
  CLOG(INFO) << "useChiselCache=" << useChiselCache;

  std::shared_ptr<grpc::Channel> grpc_channel;
  if (useGRPC) {
    CLOG(INFO) << "gRPC server=" << args->GRPCServer();

    const auto& tls_config = collectorConfig["tlsConfig"];

    std::shared_ptr<grpc::ChannelCredentials> creds = grpc::InsecureChannelCredentials();
    if (!tls_config.isNull()) {
      std::string ca_cert_path = tls_config["caCertPath"].asString();
      std::string client_cert_path = tls_config["clientCertPath"].asString();
      std::string client_key_path = tls_config["clientKeyPath"].asString();

      if (!ca_cert_path.empty() && !client_cert_path.empty() && !client_key_path.empty()) {
        creds = collector::TLSCredentialsFromFiles(ca_cert_path, client_cert_path, client_key_path);
      } else {
        CLOG(ERROR)
           << "Partial TLS config: CACertPath=" << ca_cert_path << ", ClientCertPath=" << client_cert_path
           << ", ClientKeyPath=" << client_key_path << "; will not use TLS";
      }
    }

    grpc_channel = collector::CreateChannel(args->GRPCServer(), creds);
  }

  CollectorConfig config;
  config.hostname = GetHostname();
  config.host_proc = GetHostPath("/proc");
  config.snapLen = 0;
  config.useChiselCache = useChiselCache;
  config.chisel = chisel;
  config.grpc_channel = std::move(grpc_channel);

  // Register signal handlers
  signal(SIGABRT, AbortHandler);
  signal(SIGSEGV, AbortHandler);
  signal(SIGTERM, ShutdownHandler);
  signal(SIGINT, ShutdownHandler);

  CollectorService collector(config, &g_control, &g_signum);
  collector.RunForever();

  CLOG(INFO) << "Collector exiting successfully!";

  return 0;
}
