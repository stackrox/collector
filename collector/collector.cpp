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

#include <fstream>
#include <iostream>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <string>
#include <sys/wait.h>
#include <thread>

#include "civetweb/CivetServer.h"
#include "prometheus/exposer.h"
#include "prometheus/registry.h"

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
}

#include "ChiselConsumer.h"
#include "CollectorArgs.h"
#include "EventNames.h"
#include "GetNetworkHealthStatus.h"
#include "GetStatus.h"
#include "LogLevel.h"
#include "Logging.h"
#include "SysdigService.h"
#include "CollectorStatsExporter.h"
#include "Utility.h"

#define finit_module(fd, opts, flags) syscall(__NR_finit_module, fd, opts, flags)
#define delete_module(name, flags) syscall(__NR_delete_module, name, flags)

// Set g_terminate to terminate the entire program.
static bool g_terminate;
// Set g_interrupt_sysdig to reload the sysdig configuration, or to terminate if g_terminate is also set.
static std::atomic_bool g_interrupt_sysdig;
// Store the current copy of the chisel.
static std::string g_chiselContents;

using namespace collector;

static void
signal_callback(int signal)
{
    CLOG(WARNING) << "Caught signal " << signal;
    g_terminate = true;
    g_interrupt_sysdig.store(true);
}


static void
sigsegv_handler(int signal) {
    void* array[32];
    size_t size;
    char** strings;
    unsigned int i = 0;

    size = backtrace(array, 32);

    strings = backtrace_symbols(array, size);
    FILE* fp = fopen("/host/dev/collector-stacktrace-for-ya.txt", "w");
    fprintf(stdout, "%s\n", strsignal(signal));
    fprintf(fp, "%s\n", strsignal(signal));
    for (i = 0; i < size; i++) {
        fprintf(fp, "  %s\n", strings[i]);
        fprintf(stdout, "  %s\n", strings[i]);
    }
    fflush(fp);
    fclose(fp);

    exit(EXIT_FAILURE);
}

std::string GetHostPath(const std::string& file) {
    const char* host_root = getenv("SYSDIG_HOST_ROOT");
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
void insertModule(Json::Value collectorConfig) {
    std::unordered_map<std::string, std::string> module_args;

    std::string& syscall_ids = module_args["s_syscallIds"];
    // Iterate over the syscalls that we want and pull each of their ids.
    // These are stashed into a string that will get passed to init_module
    // to insert the kernel module
    const EventNames& event_names = EventNames::GetInstance();
    for (auto itr: collectorConfig["syscalls"]) {
        string syscall = itr.asString();
        for (ppm_event_type id : event_names.GetEventIDs(syscall)) {
            syscall_ids += std::to_string(id) + ",";
        }
    }
    syscall_ids += "-1";

    module_args["exclude_initns"] = "1";
    module_args["exclude_selfns"] = "1";

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
    return (isalnum(c) || (c == '+') || (c == '/'));
}

std::string base64_encode(unsigned char const* bytes_to_encode, unsigned int in_len) {
    std::string ret;
    int i = 0;
    int j = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];

    while (in_len--) {
        char_array_3[i++] = *(bytes_to_encode++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for(i = 0; (i <4) ; i++)
                ret += base64_chars[char_array_4[i]];
            i = 0;
        }
    }

    if (i)
    {
        for(j = i; j < 3; j++)
            char_array_3[j] = '\0';

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;

        for (j = 0; (j < i + 1); j++)
            ret += base64_chars[char_array_4[j]];

        while((i++ < 3))
            ret += '=';

    }

    return ret;
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

void
writeChisel(std::string chiselName, std::string chiselContents) {
    std::ofstream out(chiselName.c_str());
    out << chiselContents;
    out.close();
}

void
registerSignalHandlers() {
    // Register signal handlers only after sysdig initialization, since sysdig also registers for
    // these signals, which prevents exiting when SIGTERM is received.
    signal(SIGINT, signal_callback);
    signal(SIGTERM, signal_callback);
    signal(SIGSEGV, sigsegv_handler);
    signal(SIGABRT, sigsegv_handler);
}

void
handleNewChisel(std::string newChisel) {
    CLOG(INFO) << "Processing new chisel.";
    g_chiselContents = newChisel;
    g_interrupt_sysdig.store(true);
}

std::string GetHostname() {
    std::string hostname_file(GetHostPath("/etc/hostname"));
    std::ifstream is(hostname_file);
    if (!is.is_open()) {
      CLOG(ERROR) << "Failed to open " << hostname_file << " to read hostname";
      return "unknown";
    }
    std::string hostname;
    is >> hostname;
    return hostname;
}

void
startChiselConsumer(std::string initialChisel, bool *g_terminate, std::string topic, std::string hostname, std::string brokers) {
  ChiselConsumer chiselConsumer(initialChisel, g_terminate);
  chiselConsumer.setCallback(handleNewChisel);
  chiselConsumer.runForever(brokers, topic, hostname);
}

void
startChild(const CollectorConfig& config) {
    insertModule(config.collectorConfig);

    // Start monitoring services.
    // Some of these variables must remain in scope, so
    // be cautious if decomposing to a separate function.
    const char *options[] = { "listening_ports", "8080", 0};
    CivetServer server(options);

    SysdigService sysdig;
    GetStatus getStatus(GetHostname(), &sysdig);

    std::shared_ptr<prometheus::Registry> registry = std::make_shared<prometheus::Registry>();

    GetNetworkHealthStatus getNetworkHealthStatus(config.brokerList, registry);

    server.addHandler("/ready", getStatus);
    server.addHandler("/networkHealth", getNetworkHealthStatus);
    LogLevel setLogLevel;
    server.addHandler("/loglevel", setLogLevel);
    // TODO(cg): Can we expose chisel contents here?

    // TODO(cg): Implement a way to not have these disappear after child restart.
    prometheus::Exposer exposer("9090");
    exposer.RegisterCollectable(registry);

    CLOG(INFO) << "Initializing signal reader...";
    // write out chisel file from incoming chisel
    writeChisel(config.chiselName, g_chiselContents);
    CLOG(INFO) << config.chiselName << " contents set to: " << g_chiselContents;

    sysdig.Init(config);

    if (!getNetworkHealthStatus.start()) {
        CLOG(FATAL) << "Unable to start network health status";
    }

    CollectorStatsExporter exporter(registry, &sysdig);
    if (!exporter.start()) {
        CLOG(FATAL) << "Unable to start sysdig stats exporter";
    }

  #ifndef COLLECTOR_CORE
    registerSignalHandlers();
  #endif /* COLLECTOR_CORE */

    CLOG(INFO) << "Starting signal reader...";
    sysdig.RunForever(g_interrupt_sysdig);
    CLOG(INFO) << "Interrupted signal reader; cleaning up...";
    exporter.stop();
    server.close();
    getNetworkHealthStatus.stop();
    sysdig.CleanUp();
    CLOG(INFO) << "Cleaned up signal reader; terminating...";

    exit(EXIT_SUCCESS);
}

int
main(int argc, char **argv)
{
    using std::string;
    using std::signal;

    collector::logging::SetGlobalLogPrefix("[Parent] ");

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
        CLOG(FATAL) << "setrlimit() failed: " << strerror(errno);
    }
#endif

    const string chiselName = "default.chisel.lua";

    CLOG(INFO) << "Starting sysdig with the following parameters: chiselName="
         << chiselName
         << ", brokerList="
         << args->BrokerList();

    // insert the kernel module with options from the configuration
    Json::Value collectorConfig = args->CollectorConfig();

    bool useKafka = !args->BrokerList().empty();
    if (!useKafka) {
        CLOG(INFO) << "Kafka is disabled.";
    }
    std::string format = "";
    if (!collectorConfig["format"].isNull()) {
        format = collectorConfig["format"].asString();
    }
    std::string networkSignalOutput = "stdout:NET :";
    if (!collectorConfig["networkSignalOutput"].isNull()) {
        networkSignalOutput = collectorConfig["networkSignalOutput"].asString();
    }
    std::string processSignalOutput = "stdout:PROC:";
    if (!collectorConfig["processSignalOutput"].isNull()) {
        processSignalOutput = collectorConfig["processSignalOutput"].asString();
    }
    std::string fileSignalOutput = "stdout:FILE:";
    if (!collectorConfig["fileSignalOutput"].isNull()) {
        fileSignalOutput = collectorConfig["fileSignalOutput"].asString();
    }
    std::string chiselsTopic = "collector-chisels-kafka-topic";
    if (!collectorConfig["chiselsTopic"].isNull()) {
        chiselsTopic = collectorConfig["chiselsTopic"].asString();
    }

    // formatters
    std::string networkSignalFormat = "network_legacy";
    if (!collectorConfig["networkSignalFormat"].isNull()) {
        networkSignalFormat = collectorConfig["networkSignalFormat"].asString();
    }
    std::string processSignalFormat = "process_legacy";
    if (!collectorConfig["processSignalFormat"].isNull()) {
        processSignalFormat = collectorConfig["processSignalFormat"].asString();
    }
    std::string fileSignalFormat = "file_legacy";
    if (!collectorConfig["fileSignalFormat"].isNull()) {
        fileSignalFormat = collectorConfig["fileSignalFormat"].asString();
    }

    // Iterate over the process syscalls
    std::string processSyscalls;
    for (auto itr: collectorConfig["process_syscalls"]) {
        if (!processSyscalls.empty()) {
            processSyscalls += ",";
	    }

        processSyscalls += itr.asString();
    }

    CLOG(INFO) << "Output specs set to: network='" << networkSignalOutput << "', process='"
               << processSignalOutput << "', file='" << fileSignalOutput << "'";
    CLOG(INFO) << "Chisels topic set to: " << chiselsTopic;

    std::string chiselB64 = args->Chisel();
    g_chiselContents = base64_decode(chiselB64);

    // Handle updates to the chisel, which can be sent to us on Kafka.
    if (useKafka) {
        std::thread chiselThread(startChiselConsumer, g_chiselContents, &g_terminate, chiselsTopic, GetHostname(), args->BrokerList());
        chiselThread.detach();
    }

    // Extract configuration options
    bool useChiselCache = collectorConfig["useChiselCache"].asBool();
    CLOG(INFO) << "useChiselCache=" << useChiselCache;

    int snapLen = 2048;
    if (!collectorConfig["signalBufferSize"].isNull() && collectorConfig["signalBufferSize"].asInt() >= 0) {
        snapLen = collectorConfig["signalBufferSize"].asInt();
    }
    CLOG(INFO) << "signalBufferSize=" << snapLen;

    CollectorConfig config;
    config.collectorConfig = collectorConfig;
    config.useKafka = useKafka;
    config.snapLen = snapLen;
    config.useChiselCache = useChiselCache;
    config.chiselName = chiselName;
    config.brokerList = args->BrokerList();
    config.format = format;
    config.networkSignalOutput = networkSignalOutput;
    config.processSignalOutput = processSignalOutput;
    config.fileSignalOutput = fileSignalOutput;
    config.processSyscalls = processSyscalls;
    config.networkSignalFormat = networkSignalFormat;
    config.processSignalFormat = processSignalFormat;
    config.fileSignalFormat = fileSignalFormat;

    for (;!g_terminate;) {
        int pid = fork();
        if (pid < 0) {
            CLOG(FATAL) << "Failed to fork.";
        } else if (pid == 0) {
            collector::logging::SetGlobalLogPrefix("[Child]  ");
            CLOG(INFO) << "Signal reader started.";
            startChild(config);
        } else {
            CLOG(INFO) << "Monitoring child process " << pid;
            for (;;) {
                if (g_interrupt_sysdig.load()) {
                    bool killed = false;
                    CLOG(INFO) << "Sending SIGTERM to " << pid;
                    kill(pid, SIGTERM);
                    for (int i = 0; i < 10; i++) {
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                        CLOG(INFO) << "Checking if process has stopped after SIGTERM (" << pid << ")";
                        pid_t killed_pid = waitpid(pid, NULL, WNOHANG);
                        if (killed_pid == pid) {
                            CLOG(INFO) << "Process has stopped (" << pid << ")";
                            killed = true;
                            break;
                        }
                    }
                    if (!killed) {
                        CLOG(WARNING) << "Process has not stopped after SIGTERM; killing (" << pid << ")";
                        kill(pid, SIGKILL);
                        CLOG(INFO) << "Waiting for process " << pid << " to exit after SIGKILL";
                        wait(NULL);
                    }
                    g_interrupt_sysdig.store(false);
                    break;
                }
                pid_t killed_pid = waitpid(pid, NULL, WNOHANG);
                if (killed_pid == pid) {
                    CLOG(ERROR) << "Process has stopped unexpectedly (" << pid << "); restarting";
                    break;
                }
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
    }
    exit(EXIT_SUCCESS);
}
