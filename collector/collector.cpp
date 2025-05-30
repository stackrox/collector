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

#include "ConfigLoader.h"

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
#include "CollectorVersion.h"
#include "Control.h"
#include "Diagnostics.h"
#include "EventNames.h"
#include "FileSystem.h"
#include "GRPC.h"
#include "GRPCUtil.h"
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
std::shared_ptr<grpc::Channel> createChannel(const CollectorConfig& config) {
  const std::string& grpc_server = *config.GetGrpcServer();
  CLOG(INFO) << "Sensor configured at address: " << grpc_server;
  const auto& tls_config = config.TLSConfiguration();

  std::shared_ptr<grpc::ChannelCredentials> creds = grpc::InsecureChannelCredentials();
  if (tls_config.has_value()) {
    if (tls_config->IsValid()) {
      creds = collector::TLSCredentialsFromConfig(*tls_config);
    } else {
      CLOG(ERROR)
          << "Partial TLS config: CACertPath=" << tls_config->GetCa() << ", ClientCertPath=" << tls_config->GetClientCert()
          << ", ClientKeyPath=" << tls_config->GetClientKey() << "; will not use TLS";
    }
  }

  return {collector::CreateChannel(grpc_server, GetSNIHostname(), creds)};
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
}

void RunService(CollectorConfig& config) {
  auto& startup_diagnostics = StartupDiagnostics::GetInstance();
  CollectorService collector(config, &g_control, &g_signum);

  if (!collector.InitKernel()) {
    startup_diagnostics.Log();
    CLOG(FATAL) << "Failed to initialize collector kernel components.";
  }

  // output the GPL notice only once the kernel object has been found or downloaded
  gplNotice();

  startup_diagnostics.Log();

  collector.RunForever();
}

int main(int argc, char** argv) {
  // Print system information before doing actual work.
  auto& host_info = HostInfo::Instance();
  CLOG(INFO) << "Collector Version: " << GetCollectorVersion();
  CLOG(INFO) << "OS: " << host_info.GetDistro();
  CLOG(INFO) << "Kernel Version: " << host_info.GetKernelVersion().GetRelease();
  CLOG(INFO) << "Architecture: " << host_info.GetKernelVersion().GetMachine();

  initialChecks();

  CollectorArgs* args = CollectorArgs::getInstance();
  int exitCode = 0;
  if (!args->parse(argc, argv, exitCode)) {
    if (!args->Message().empty()) {
      CLOG(FATAL) << args->Message();
    }
    CLOG(FATAL) << "Error parsing arguments";
  }

  CollectorConfig config;
  config.InitCollectorConfig(args);
  if (ConfigLoader(config).LoadConfiguration() == collector::ConfigLoader::PARSE_ERROR) {
    CLOG(FATAL) << "Unable to parse configuration file";
  }

  setCoreDumpLimit(config.IsCoreDumpEnabled());

  auto& startup_diagnostics = StartupDiagnostics::GetInstance();

  // Extract configuration options
  bool useGRPC = config.GetGrpcServer().has_value();
  std::shared_ptr<grpc::Channel> sensor_connection;

  if (useGRPC) {
    sensor_connection = createChannel(config);
    CLOG(INFO) << "Attempting to connect to Sensor";
    if (attemptGRPCConnection(sensor_connection)) {
      CLOG(INFO) << "Successfully connected to Sensor.";
    } else {
      startup_diagnostics.Log();
      CLOG(FATAL) << "Unable to connect to Sensor at '" << config.GetGrpcServer().value() << "'.";
    }
    startup_diagnostics.ConnectedToSensor();
  } else {
    CLOG(INFO) << "GRPC is disabled. Specify GRPC_SERVER='server addr' env and signalFormat = 'signal_summary' and  signalOutput = 'grpc'";
  }

  // Register signal handlers
  signal(SIGABRT, AbortHandler);
  signal(SIGSEGV, AbortHandler);
  signal(SIGTERM, ShutdownHandler);
  signal(SIGINT, ShutdownHandler);

  config.grpc_channel = std::move(sensor_connection);

  RunService(config);

  CLOG(INFO) << "Collector exiting successfully!";

  return 0;
}
