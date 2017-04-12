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
#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <string>

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

#include "CollectorArgs.h"
#include "GetStatus.h"
#include "LogLevel.h"
#include "SysdigService.h"
#include "SysdigStatsExporter.h"

#define init_module(mod, len, opts) syscall(__NR_init_module, mod, len, opts)
#define delete_module(name, flags) syscall(__NR_delete_module, name, flags)

static bool g_terminate;

using namespace collector;

static void
signal_callback(int signal)
{
    std::cerr << "Caught signal " << signal << std::endl;
    g_terminate = true;
    exit(EXIT_SUCCESS);
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

    exit(1);
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
        std::cerr << "Cannot open kernel module: " <<
            SysdigService::modulePath << ". Aborting..." << std::endl;
        exit(-1);
    }
    struct stat st;
    if (fstat(fd, &st) < 0) {
        std::cerr << "Error getting file info for kernel module: " <<
            SysdigService::modulePath << ". Aborting..." << std::endl;
        exit(-1);
    }
    size_t imageSize = st.st_size;
    char *image = new char[imageSize];
    read(fd, image, imageSize);
    close(fd);

    std::cout << "Inserting kernel module " << SysdigService::modulePath <<
        " with indefinite removal and retry if required, and with arguments " << args << endl;

    // Attempt to insert the module. If it is already inserted then remove it and
    // try again
    int result = init_module(image, imageSize, args.c_str());
    while (result != 0) {
        if (result == EEXIST) {
            // note that we forcefully remove the kernel module whether or not it has a non-zero
            // reference count. There is only one container that is ever expected to be using
            // this kernel module and that is us
            delete_module(SysdigService::moduleName.c_str(), O_NONBLOCK | O_TRUNC);
            sleep(2);    // wait for 2s before trying again
        } else {
            std::cerr << "Error inserting kernel module: " <<
                SysdigService::modulePath << ": " << strerror(errno) <<". Aborting..." << std::endl;
            exit(-1);
        }
        result = init_module(image, imageSize, args.c_str());
    }

    std::cout << "Done inserting kernel module " << SysdigService::modulePath << "!" << endl;

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
writeChisel(std::string chiselName, std::string chisel64) {
    std::string chisel = base64_decode(chisel64);
    cout << "Chisel post decode:\n" << chisel << endl;

    std::ofstream out(chiselName.c_str());
    out << chisel;
    out.close();
}

int
main(int argc, char **argv)
{
    using std::cerr;
    using std::cout;
    using std::endl;
    using std::string;
    using std::signal;

    LogLevel setLogLevel;
    setLogLevel.stdBuf = std::cout.rdbuf();
    ofstream nullStream("/dev/null");
    setLogLevel.nullBuf = nullStream.rdbuf();

    std::cout.rdbuf(setLogLevel.nullBuf);

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

    cout << "Hostname detected: " << hostname << endl;

    const string chiselName = "default.chisel.lua";
    const int snapLen = 2048;

    cout << "Starting sysdig with the following parameters: chiselName="
         << chiselName
         << ", brokerList="
         << args->BrokerList()
         << ", snapLen="
         << snapLen
         << endl;

    SysdigService sysdig(g_terminate);

    // insert the kernel module with options from the configuration
    Json::Value collectorConfig = args->CollectorConfig();
    insertModule(sysdig, collectorConfig);

    bool useKafka = true;
    if (collectorConfig["output"].isNull() || collectorConfig["output"] == "stdout") {
        useKafka = false;
    }
    std::string format = "";
    if (!collectorConfig["format"].isNull()) {
        format = collectorConfig["format"].asString();
    }
    std::string defaultTopic = "sysdig-kafka-topic";
    if (!collectorConfig["defaultTopic"].isNull()) {
        defaultTopic = collectorConfig["defaultTopic"].asString();
    }
    std::string networkTopic = collectorConfig["networkTopic"].asString();
    if (networkTopic.empty()) {
        cerr << "Network topic not specified" << endl;
        exit(-1);
    }

    // write out chisel file from incoming chisel
    writeChisel(chiselName, args->Chisel());

    cout << chiselName << ", " << format << ", " << defaultTopic << ", "  << networkTopic << endl;

    int code = sysdig.init(chiselName, args->BrokerList(), format, useKafka, defaultTopic,
                            networkTopic, snapLen);
    if (code != 0) {
        cerr << "Unable to initialize sysdig" << endl;
        exit(code);
    }

#ifndef COLLECTOR_CORE
    // Register signal handlers only after sysdig initialization since sysdig also registers for
    // these signals which prevents exiting when SIGTERM is received.
    signal(SIGINT, signal_callback);
    signal(SIGTERM, signal_callback);
    signal(SIGSEGV, sigsegv_handler);
#endif

    const char *options[] = { "listening_ports", "4419", 0};
    CivetServer server(options);

    GetStatus getStatus(&sysdig);
    server.addHandler("/status", getStatus);
    server.addHandler("/loglevel", setLogLevel);

    prometheus::Exposer exposer("4418");
    std::shared_ptr<prometheus::Registry> registry = std::make_shared<prometheus::Registry>();
    exposer.RegisterCollectable(registry);

    SysdigStatsExporter exporter(registry, &sysdig);
    if (!exporter.start()) {
        cerr << "Unable to start sysdig stats exporter" << endl;
        exit(EXIT_FAILURE);
    }

    sysdig.runForever();
    sysdig.cleanup();
    exit(EXIT_SUCCESS);
}
