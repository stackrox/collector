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
#include <cctype>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

#include <sys/wait.h>

extern "C" {
#include <assert.h>
#include <cap-ng.h>
#include <cxxabi.h>
#include <dlfcn.h>
#include <execinfo.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
}
#include "CollectorArgs.h"
#include "CollectorService.h"
#include "CollectorStatsExporter.h"
#include "Control.h"
#include "Diagnostics.h"
#include "EventNames.h"
#include "FileSystem.h"
#include "GRPC.h"
#include "GRPCUtil.h"
#include "GetKernelObject.h"
#include "GetStatus.h"
#include "HostInfo.h"
#include "LogLevel.h"
#include "Logging.h"
#include "SysdigService.h"
#include "Utility.h"

static const int MAX_GRPC_CONNECTION_POLLS = 30;

using namespace collector;

static std::atomic<ControlValue> g_control(ControlValue::RUN);
static std::atomic<int> g_signum(0);

static StartupDiagnostics g_startup_diagnostics;

static void
ShutdownHandler(int signum) {
  // Only set the control variable; the collector service will take care of the rest.
  g_signum.store(signum);
  g_control.store(ControlValue::STOP_COLLECTOR);
}

static void WriteBuffer(char* buffer, int num_bytes, int buffer_size) {
  if (num_bytes > buffer_size) {
    // the output was truncated, to keep it readable add a new line
    buffer[buffer_size - 1] = '\n';
    write(STDERR_FILENO, buffer, buffer_size);
  } else {
    write(STDERR_FILENO, buffer, num_bytes);
  }
}

static void AbortHandler(int signum) {
  // Write a stacktrace to stderr. Since the abort handler could be called
  // after SIGSEGV or some other emergency situations:
  // * we have to be modest and use little memory (in case if the original
  //   reason was OOM)
  // * as the original implementation noted, use reentrant functions
  //
  // The latter one was refering to snprintf, although it's not clear if it is
  // reentrant. Libc docs says functions using I/O streams are potentially
  // non-reentrant [1], and it seems under the hood snprintf uses streams.
  //
  // [1]: https://www.gnu.org/savannah-checkouts/gnu/libc/manual/html_node/Nonreentrancy.html

  void* buffer[32];  // Addresses buffer

  char message_buffer[256];  // Actual text buffer for a single stack trace
                             // address. Some templated signatures could be
                             // particularly long.
  int num_bytes;

  size_t n_frames = backtrace(buffer, 32);
  size_t max_frames = sizeof(buffer) / sizeof(buffer[0]);
  int message_buffer_size = sizeof(message_buffer);

  for (size_t i = 1; i < n_frames; i++) {
    Dl_info info;

    if (dladdr(buffer[i], &info)) {
      // Try to demangle the stacktrace
      char* demangled = NULL;
      int status;
      size_t shift_from_symbol;

      // NULL as a second argument for the output buffer will make it allocate
      // new region of memory
      demangled = abi::__cxa_demangle(info.dli_sname, NULL, NULL, &status);
      shift_from_symbol = (char*)buffer[i] - (char*)info.dli_saddr;

      // The pattern is:
      // - file name of the object
      // - demangled or original name of the obect (or "(null)" if NULL)
      // - address of the object
      // - distance to the closest symbol
      num_bytes = snprintf(message_buffer, message_buffer_size,
                           "%s %s %p + %zd\n", info.dli_fname,
                           status == 0 ? demangled : info.dli_sname, buffer[i],
                           info.dli_saddr != NULL ? shift_from_symbol : 0);
      free(demangled);
      WriteBuffer(message_buffer, num_bytes, message_buffer_size);

    } else {
      // Failed to translate the address into symbolic information,
      // at least print the raw address value.
      num_bytes = snprintf(message_buffer, message_buffer_size, "%p\n", buffer[i]);
      WriteBuffer(message_buffer, num_bytes, message_buffer_size);
    }
  }

  if (n_frames == max_frames)
    write(STDERR_FILENO, "[truncated]\n", 14);

  // Write a message to stderr using only reentrant functions.
  num_bytes = snprintf(message_buffer, message_buffer_size,
                       "Caught signal %d (%s): %s\n",
                       signum, SignalName(signum), strsignal(signum));
  WriteBuffer(message_buffer, num_bytes, message_buffer_size);

  // Re-raise the signal (this time routing it to the default handler) to make
  // sure we get the correct exit code.
  signal(signum, SIG_DFL);
  raise(signum);
}

// creates a GRPC channel, using the tls configuration provided from the args.
std::shared_ptr<grpc::Channel> createChannel(CollectorArgs* args) {
  CLOG(INFO) << "Sensor configured at address: " << args->GRPCServer();
  Json::Value collectorConfig = args->CollectorConfig();

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

  return {collector::CreateChannel(args->GRPCServer(), GetSNIHostname(), creds)};
}

// attempts to connect to the GRPC server, up to a timeout
bool attemptGRPCConnection(std::shared_ptr<grpc::Channel>& channel) {
  int polls = MAX_GRPC_CONNECTION_POLLS;
  auto poll_check = [&polls] {
    // this closure needs to return false until we wish to stop
    // polling, so keep going until we hit 0
    return (polls-- == 0);
  };
  return WaitForChannelReady(channel, poll_check);
}

void setCoreDumpLimit(bool enableCoreDump) {
  struct rlimit limit;
  limit.rlim_cur = 0;
  limit.rlim_max = 0;
  if (enableCoreDump) {
    CLOG(DEBUG) << "Attempting to enable core dumps";
    limit.rlim_cur = RLIM_INFINITY;
    limit.rlim_max = RLIM_INFINITY;
  } else {
    CLOG(DEBUG) << "Core dumps not enabled";
  }
  if (setrlimit(RLIMIT_CORE, &limit) != 0) {
    CLOG(ERROR) << "Failed to enable core dumps: " << StrError();
  }
}

void gplNotice() {
  CLOG(INFO) << "";
  CLOG(INFO) << "This product uses kernel module and ebpf subcomponents licensed under the GNU";
  CLOG(INFO) << "GENERAL PURPOSE LICENSE Version 2 outlined in the /kernel-modules/LICENSE file.";
  CLOG(INFO) << "Source code for the kernel module and ebpf subcomponents is available upon";
  CLOG(INFO) << "request by contacting support@stackrox.com.";
  CLOG(INFO) << "";
}

void initialChecks() {
  if (!g_control.is_lock_free()) {
    CLOG(FATAL) << "Internal error: could not create a lock-free control variable.";
  }

  struct stat st;
  if (stat("/module", &st) != 0 || !S_ISDIR(st.st_mode)) {
    CLOG(FATAL) << "Internal error: /module directory does not exist.";
  }
}

bool downloadKernelDriver(const CollectorArgs* args, CollectorConfig& config) {
  CLOG(INFO) << "Module version: " << GetModuleVersion();

  std::vector<std::string> kernel_candidates = GetKernelCandidates();

  if (kernel_candidates.empty()) {
    CLOG(ERROR) << "No kernel candidates available";
    return false;
  }

  struct {
    std::string path;
    std::string name;
    std::string extension;
    std::string type;
  } kernel_object;

  if (config.UseEbpf()) {
    kernel_object.path = SysdigService::kProbePath;
    kernel_object.name = SysdigService::kProbeName;
    kernel_object.extension = ".o";
    kernel_object.type = "eBPF probe";
  } else {
    kernel_object.path = SysdigService::kModulePath;
    kernel_object.name = SysdigService::kModuleName;
    kernel_object.extension = ".ko";
    kernel_object.type = "kernel module";
  }

  CLOG(INFO) << "Attempting to find " << kernel_object.type << " - Candidate kernel versions: ";
  for (auto candidate : kernel_candidates) {
    CLOG(INFO) << candidate;
  }

  bool success = false;
  std::string kernel_candidate;

  for (auto kernel_candidate : kernel_candidates) {
    std::string kernel_module = kernel_object.name + "-" + kernel_candidate + kernel_object.extension;

    success = GetKernelObject(args->GRPCServer(), config.TLSConfiguration(), kernel_module, kernel_object.path,
                              config.CurlVerbose());
    if (!success) {
      CLOG(ERROR) << "Error getting kernel object: " << kernel_module;
    } else {
      break;
    }
  }

  return success;
}

int main(int argc, char** argv) {
  initialChecks();

  CollectorArgs* args = CollectorArgs::getInstance();
  int exitCode = 0;
  if (!args->parse(argc, argv, exitCode)) {
    if (!args->Message().empty()) {
      CLOG(FATAL) << args->Message();
    }
    CLOG(FATAL) << "Error parsing arguments";
  }

  CollectorConfig config(args);

  setCoreDumpLimit(config.IsCoreDumpEnabled());

  // Extract configuration options
  bool useGRPC = !args->GRPCServer().empty();
  std::shared_ptr<grpc::Channel> sensor_connection;

  if (useGRPC) {
    sensor_connection = createChannel(args);
    CLOG(INFO) << "Attempting to connect to Sensor";
    if (attemptGRPCConnection(sensor_connection)) {
      CLOG(INFO) << "Successfully connected to Sensor.";
    } else {
      g_startup_diagnostics.Log();
      CLOG(FATAL) << "Unable to connect to Sensor at '" << args->GRPCServer() << "'.";
    }
    g_startup_diagnostics.ConnectedToSensor();
  } else {
    CLOG(INFO) << "GRPC is disabled. Specify GRPC_SERVER='server addr' env and signalFormat = 'signal_summary' and  signalOutput = 'grpc'";
  }

  if (!downloadKernelDriver(args, config)) {
    g_startup_diagnostics.Log();
    HostInfo& host = HostInfo::Instance();
    CLOG(FATAL) << "No suitable kernel object downloaded for kernel " << host.GetKernelVersion().release;
  }

  g_startup_diagnostics.KernelDriverDownloaded();

  // output the GPL notice only once the kernel object has been found or downloaded
  gplNotice();

  // Register signal handlers
  signal(SIGABRT, AbortHandler);
  signal(SIGSEGV, AbortHandler);
  signal(SIGTERM, ShutdownHandler);
  signal(SIGINT, ShutdownHandler);

  config.grpc_channel = std::move(sensor_connection);

  CollectorService collector(config, &g_control, &g_signum);

  if (!collector.InitKernel()) {
    g_startup_diagnostics.Log();
    CLOG(FATAL) << "Failed to initialize collector kernel components.";
  }

  g_startup_diagnostics.KernelDriverLoaded();
  g_startup_diagnostics.Log();

  collector.RunForever();

  CLOG(INFO) << "Collector exiting successfully!";

  return 0;
}
