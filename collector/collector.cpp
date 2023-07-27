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
#include "AbortHandler.h"
#include "CollectorArgs.h"
#include "CollectorService.h"
#include "CollectorStatsExporter.h"
#include "Control.h"
#include "Diagnostics.h"
#include "DriverCandidates.h"
#include "EventNames.h"
#include "FileSystem.h"
#include "GRPC.h"
#include "GRPCUtil.h"
#include "GetKernelObject.h"
#include "GetStatus.h"
#include "HostInfo.h"
#include "LogLevel.h"
#include "Logging.h"
#include "Utility.h"

static const int MAX_GRPC_CONNECTION_POLLS = 30;

using namespace collector;

static std::atomic<ControlValue> g_control(ControlValue::RUN);
static std::atomic<int> g_signum(0);

static void
ShutdownHandler(int signum) {
  // Only set the control variable; the collector service will take care of the rest.
  g_signum.store(signum);
  g_control.store(ControlValue::STOP_COLLECTOR);
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
  CLOG(INFO) << "This product uses ebpf subcomponents licensed under the GNU";
  CLOG(INFO) << "GENERAL PURPOSE LICENSE Version 2 outlined in the /kernel-modules/LICENSE file.";
  CLOG(INFO) << "Source code for the ebpf subcomponents is available at";
  CLOG(INFO) << "https://github.com/stackrox/falcosecurity-libs/";
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

  auto& startup_diagnostics = StartupDiagnostics::GetInstance();

  // Extract configuration options
  bool useGRPC = !args->GRPCServer().empty();
  std::shared_ptr<grpc::Channel> sensor_connection;

  if (useGRPC) {
    sensor_connection = createChannel(args);
    CLOG(INFO) << "Attempting to connect to Sensor";
    if (attemptGRPCConnection(sensor_connection)) {
      CLOG(INFO) << "Successfully connected to Sensor.";
    } else {
      startup_diagnostics.Log();
      CLOG(FATAL) << "Unable to connect to Sensor at '" << args->GRPCServer() << "'.";
    }
    startup_diagnostics.ConnectedToSensor();
  } else {
    CLOG(INFO) << "GRPC is disabled. Specify GRPC_SERVER='server addr' env and signalFormat = 'signal_summary' and  signalOutput = 'grpc'";
  }

  CLOG(INFO) << "Module version: " << GetModuleVersion();

  // Register signal handlers
  signal(SIGABRT, AbortHandler);
  signal(SIGSEGV, AbortHandler);
  signal(SIGTERM, ShutdownHandler);
  signal(SIGINT, ShutdownHandler);

  config.grpc_channel = std::move(sensor_connection);

  CollectorService collector(config, &g_control, &g_signum);

  if (!SetupKernelDriver(collector, args->GRPCServer(), config)) {
    startup_diagnostics.Log();
    CLOG(FATAL) << "Failed to initialize collector kernel components.";
  }

  // output the GPL notice only once the kernel object has been found or downloaded
  gplNotice();

  startup_diagnostics.Log();

  collector.RunForever();

  CLOG(INFO) << "Collector exiting successfully!";

  return 0;
}
