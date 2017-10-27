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

const char* scap_get_host_root();
}

#include "ChiselConsumer.h"
#include "CollectorArgs.h"
#include "GetNetworkHealthStatus.h"
#include "GetStatus.h"
#include "LogLevel.h"
#include "SysdigService.h"
#include "CollectorStatsExporter.h"

#define init_module(mod, len, opts) syscall(__NR_init_module, mod, len, opts)
#define delete_module(name, flags) syscall(__NR_delete_module, name, flags)

// Set g_terminate to terminate the entire program.
static bool g_terminate;
// Set g_interrupt_sysdig to reload the sysdig configuration, or to terminate if g_terminate is also set.
static bool g_interrupt_sysdig;
// Store the current copy of the chisel.
static std::string g_chiselContents;

using namespace collector;

static void
signal_callback(int signal)
{
    std::cerr << "Caught signal " << signal << std::endl;
    g_terminate = true;
    g_interrupt_sysdig = true;
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
    for (i = 0; i < size; i++) {
        fprintf(fp, "  %s\n", strings[i]);
        fprintf(stdout, "  %s\n", strings[i]);
    }
    fflush(fp);
    fclose(fp);

    exit(EXIT_FAILURE);
}

static const std::string
getHostname()
{
    std::string hostname;
    const std::string file = std::string(scap_get_host_root()) + "/etc/hostname";
    std::ifstream f(file.c_str());
    f >> hostname;
    f.close();
    return hostname;
}

static const std::string hostname(getHostname());

// insertModule
// Method to insert the kernel module. The options to the module are computed
// from the collector configuration. Specifically, the syscalls that we should
// extract
void insertModule(SysdigService& sysdigService, Json::Value collectorConfig) {
    std::string args = "s_syscallIds=";

    // Iterate over the syscalls that we want and pull each of their ids.
    // These are stashed into a string that will get passed to init_module
    // to insert the kernel module
    for (auto itr: collectorConfig["syscalls"]) {
        string syscall = itr.asString();
        std::vector<int> ids;
        sysdigService.getSyscallIds(syscall, ids);
        for (unsigned int i = 0; i < ids.size(); i++) {
            std::string strid = std::to_string(ids[i]);
            args += (strid + ",");
        }
    }
    args += "-1";

    int fd = open(SysdigService::modulePath.c_str(), O_RDONLY);
    if (fd < 0) {
        std::cerr << "[Child]  Cannot open kernel module: " <<
            SysdigService::modulePath << ". Aborting..." << std::endl;
        exit(EXIT_FAILURE);
    }
    struct stat st;
    if (fstat(fd, &st) < 0) {
        std::cerr << "[Child]  Error getting file info for kernel module: " <<
            SysdigService::modulePath << ". Aborting..." << std::endl;
        exit(EXIT_FAILURE);
    }
    size_t imageSize = st.st_size;
    char *image = new char[imageSize];
    read(fd, image, imageSize);
    close(fd);

    std::cerr << "[Child]  Inserting kernel module " << SysdigService::modulePath <<
        " with indefinite removal and retry if required." << std::endl;
    std::cout << "[Child]  Kernel module arguments: " << args << std::endl;

    // Attempt to insert the module. If it is already inserted then remove it and
    // try again
    int result = init_module(image, imageSize, args.c_str());
    while (result != 0) {
        if (errno == EEXIST) {
            // note that we forcefully remove the kernel module whether or not it has a non-zero
            // reference count. There is only one container that is ever expected to be using
            // this kernel module and that is us
            delete_module(SysdigService::moduleName.c_str(), O_NONBLOCK | O_TRUNC);
            sleep(2);    // wait for 2s before trying again
        } else {
            std::cerr << "[Child]  Error inserting kernel module: " <<
                SysdigService::modulePath << ": " << strerror(errno) <<". Aborting..." << std::endl;
            exit(EXIT_FAILURE);
        }
        result = init_module(image, imageSize, args.c_str());
    }

    std::cerr << "[Child]  Done inserting kernel module " << SysdigService::modulePath << "." << endl;

    delete[] image;
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
}

void
handleNewChisel(std::string newChisel) {
    cerr << "Processing new chisel." << endl;
    g_chiselContents = newChisel;
    g_interrupt_sysdig = true;
}

void
startChiselConsumer(std::string initialChisel, bool *g_terminate, std::string topic, std::string hostname, std::string brokers) {
  ChiselConsumer chiselConsumer(initialChisel, g_terminate);
  chiselConsumer.setCallback(handleNewChisel);
  chiselConsumer.runForever(brokers, topic, hostname);
}

void
startChild(std::string chiselName, std::string brokerList,
           std::string format, bool useKafka, std::string defaultTopic,
           std::string networkTopic, std::string processTopic, std::string processSyscalls,
           int snapLen, Json::Value collectorConfig) {
    SysdigService sysdig(g_interrupt_sysdig);

    insertModule(sysdig, collectorConfig);

    LogLevel setLogLevel;
    setLogLevel.stdBuf = std::cout.rdbuf();
    ofstream nullStream("/dev/null");
    setLogLevel.nullBuf = nullStream.rdbuf();

    std::cout.rdbuf(setLogLevel.nullBuf);

    // Start monitoring services.
    // Some of these variables must remain in scope, so
    // be cautious if decomposing to a separate function.
    const char *options[] = { "listening_ports", "8080", 0};
    CivetServer server(options);

    GetStatus getStatus(&sysdig);
    GetNetworkHealthStatus getNetworkHealthStatus(g_terminate);
    server.addHandler("/ready", getStatus);
    server.addHandler("/networkHealth", getNetworkHealthStatus);
    server.addHandler("/loglevel", setLogLevel);
    // TODO(cg): Can we expose chisel contents here?

    // TODO(cg): Implement a way to not have these disappear after child restart.
    prometheus::Exposer exposer("9090");
    std::shared_ptr<prometheus::Registry> registry = std::make_shared<prometheus::Registry>();
    exposer.RegisterCollectable(registry);

    getNetworkHealthStatus.start();

    CollectorStatsExporter exporter(registry, &sysdig);
    if (!exporter.start()) {
        cerr << "[Child]  Unable to start sysdig stats exporter" << endl;
        exit(EXIT_FAILURE);
    }

    cerr << "[Child]  Initializing signal reader..." << endl;
    // write out chisel file from incoming chisel
    writeChisel(chiselName, g_chiselContents);
    cerr << "[Child]  " << chiselName << " contents set to: " << g_chiselContents << endl;

    int code = sysdig.init(chiselName, brokerList, format, useKafka, defaultTopic,
                            networkTopic, processTopic, processSyscalls, snapLen);
    if (code != 0) {
        cerr << "[Child]  Unable to initialize sysdig" << endl;
        exit(code);
    }

  #ifndef COLLECTOR_CORE
    registerSignalHandlers();
  #endif /* COLLECTOR_CORE */

    cerr << "[Child]  Starting signal reader..." << endl;
    sysdig.runForever();
    cerr << "[Child]  Interrupted signal reader; cleaning up..." << endl;
    sysdig.cleanup();
    cerr << "[Child]  Cleaned up signal reader; terminating..." << endl;

    getNetworkHealthStatus.stop();

    exit(EXIT_SUCCESS);
}

int
main(int argc, char **argv)
{
    using std::cerr;
    using std::cout;
    using std::endl;
    using std::string;
    using std::signal;

    CollectorArgs *args = CollectorArgs::getInstance();
    int exitCode = 0;
    if (!args->parse(argc, argv, exitCode)) {
        if (!args->Message().empty()) {
            cerr << args->Message() << endl;
        }
        exit(exitCode);
    }

#ifdef COLLECTOR_CORE
    struct rlimit limit;
    limit.rlim_cur = RLIM_INFINITY;
    limit.rlim_max = RLIM_INFINITY;
    if (setrlimit(RLIMIT_CORE, &limit) != 0) {
        cerr << "setrlimit() failed: " << strerror(errno) << std::endl;
        exit(-1);
    }
#endif

    const string chiselName = "default.chisel.lua";
    const int snapLen = 2048;

    cerr << "Starting sysdig with the following parameters: chiselName="
         << chiselName
         << ", brokerList="
         << args->BrokerList()
         << ", snapLen="
         << snapLen
         << endl;

    // insert the kernel module with options from the configuration
    Json::Value collectorConfig = args->CollectorConfig();

    bool useKafka = true;
    if (collectorConfig["output"].isNull() || collectorConfig["output"] == "stdout") {
        cerr << "Kafka is disabled." << endl;
        useKafka = false;
    }
    std::string format = "";
    if (!collectorConfig["format"].isNull()) {
        format = collectorConfig["format"].asString();
    }
    std::string defaultTopic = "collector-kafka-topic";
    if (!collectorConfig["defaultTopic"].isNull()) {
        defaultTopic = collectorConfig["defaultTopic"].asString();
    }
    std::string networkTopic = "collector-network-kafka-topic";
    if (!collectorConfig["networkTopic"].isNull()) {
        networkTopic = collectorConfig["networkTopic"].asString();
    }
    std::string processTopic = "collector-process-kafka-topic";
    if (!collectorConfig["processTopic"].isNull()) {
        processTopic = collectorConfig["processTopic"].asString();
    }
    std::string chiselsTopic = "collector-chisels-kafka-topic";
    if (!collectorConfig["chiselsTopic"].isNull()) {
        chiselsTopic = collectorConfig["chiselsTopic"].asString();
    }
    
    // Iterate over the process syscalls
    std::string processSyscalls;
    for (auto itr: collectorConfig["process_syscalls"]) {
        if (!processSyscalls.empty()) {
            processSyscalls += ",";
	}

        processSyscalls += itr.asString();
    }

    cerr << "Output topics set to: default=" << defaultTopic << ", network=" << networkTopic << ", process=" << processTopic << endl;
    cerr << "Chisels topic set to: " << chiselsTopic << endl;

    std::string chiselB64 = args->Chisel();
    g_chiselContents = base64_decode(chiselB64);

    // Handle updates to the chisel, which can be sent to us on Kafka.
    if (useKafka) {
        std::thread chiselThread(startChiselConsumer, g_chiselContents, &g_terminate, chiselsTopic, hostname, args->BrokerList());
        chiselThread.detach();
    }

    for (;!g_terminate;) {
        int pid = fork();
        if (pid < 0) {
            cerr << "Failed to fork." << endl;
            exit(EXIT_FAILURE);
        } else if (pid == 0) {
            cerr << "[Child]  Signal reader started." << endl;
            startChild(chiselName, args->BrokerList(), format, useKafka, defaultTopic,
                       networkTopic, processTopic, processSyscalls, snapLen, collectorConfig);
        } else {
            cerr << "[Parent] Monitoring child process " << pid << endl;
            for (;;) {
                if (g_interrupt_sysdig) {
                    bool killed = false;
                    cerr << "[Parent] Sending SIGTERM to " << pid << endl;
                    kill(pid, SIGTERM);
                    for (int i = 0; i < 10; i++) {
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                        cerr << "[Parent] Checking if process has stopped after SIGTERM (" << pid << ")" << endl;
                        pid_t killed_pid = waitpid(pid, NULL, WNOHANG);
                        if (killed_pid == pid) {
                            cerr << "[Parent] Process has stopped (" << pid << ")" << endl;
                            killed = true;
                            break;
                        }
                    }
                    if (!killed) {
                        cerr << "[Parent] Process has not stopped after SIGTERM; killing (" << pid << ")" << endl;
                        kill(pid, SIGKILL);
                        cerr << "[Parent] Waiting for process " << pid << " to exit after SIGKILL" << endl;
                        wait(NULL);
                    }
                    g_interrupt_sysdig = false;
                    break;
                }
                pid_t killed_pid = waitpid(pid, NULL, WNOHANG);
                if (killed_pid == pid) {
                    cerr << "[Parent] Process has stopped unexpectedly (" << pid << "); restarting" << endl;
                    break;
                }
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
    }
    exit(EXIT_SUCCESS);
}
