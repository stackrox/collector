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

#include <iostream>
#include <map>
#include <string>
#include <vector>

extern "C" {
    #include <sys/types.h>
    #include <inttypes.h>
    #include <unistd.h>

    #include "librdkafka/rdkafka.h"
    #include "ppm_events_public.h"
}

#include "KafkaClient.h"
#include "SysdigService.h"

extern "C" {

typedef struct {
  uint64_t    nEvents;                // the number of kernel events
  uint64_t    nDrops;                 // the number of drops
  uint64_t    nPreemptions;           // the number of preemptions
  uint64_t    nFilteredEvents;        // events post chisel filter
  std::string nodeName;               // the name of this node (hostname)
} sysdigDataT;

int sysdigInitialize(string chiselName, string brokerList, string format,
                     bool useKafka, string defaultTopic, string networkTopic,
                     int snapLen);
void sysdigCleanup();
void sysdigStartProduction(bool &isInterrupted);
bool sysdigGetSysdigData(sysdigDataT& sysdigData);
bool isSysdigInitialized();

}

namespace collector {

std::string SysdigService::modulePath = "/module/collector.ko";
std::string SysdigService::moduleName = "collector";

SysdigService::SysdigService(bool &terminateFlag)
    : terminate(terminateFlag)
{
    syscallsMap["open"] = 	PPME_SYSCALL_OPEN_E;
    syscallsMap["close"] = 	PPME_SYSCALL_CLOSE_E;
    syscallsMap["read"] = 	PPME_SYSCALL_READ_E;
    syscallsMap["write"] = 	PPME_SYSCALL_WRITE_E;
    syscallsMap["brk"] = 	PPME_SYSCALL_BRK_1_E;
    syscallsMap["execve"] = 	PPME_SYSCALL_EXECVE_8_E;
    syscallsMap["clone"] = 	PPME_SYSCALL_CLONE_11_E;
    syscallsMap["procexit"] = 	PPME_PROCEXIT_E;
    syscallsMap["socket"] = 	PPME_SOCKET_SOCKET_E;
    syscallsMap["bind"] = 	PPME_SOCKET_BIND_E;
    syscallsMap["connect"] = 	PPME_SOCKET_CONNECT_E;
    syscallsMap["listen"] = 	PPME_SOCKET_LISTEN_E;
    syscallsMap["accept"] = 	PPME_SOCKET_ACCEPT_E;
    syscallsMap["send"] = 	PPME_SOCKET_SEND_E;
    syscallsMap["sendto"] = 	PPME_SOCKET_SENDTO_E;
    syscallsMap["recv"] = 	PPME_SOCKET_RECV_E;
    syscallsMap["recvfrom"] = 	PPME_SOCKET_RECVFROM_E;
    syscallsMap["shutdown"] = 	PPME_SOCKET_SHUTDOWN_E;
    syscallsMap["getsockname"] = 	PPME_SOCKET_GETSOCKNAME_E;
    syscallsMap["getpeername"] = 	PPME_SOCKET_GETPEERNAME_E;
    syscallsMap["socketpair"] = 	PPME_SOCKET_SOCKETPAIR_E;
    syscallsMap["setsockopt"] = 	PPME_SOCKET_SETSOCKOPT_E;
    syscallsMap["getsockopt"] = 	PPME_SOCKET_GETSOCKOPT_E;
    syscallsMap["sendmsg"] = 	PPME_SOCKET_SENDMSG_E;
    syscallsMap["sendmmsg"] = 	PPME_SOCKET_SENDMMSG_E;
    syscallsMap["recvmsg"] = 	PPME_SOCKET_RECVMSG_E;
    syscallsMap["recvmmsg"] = 	PPME_SOCKET_RECVMMSG_E;
    syscallsMap["accept"] = 	PPME_SOCKET_ACCEPT_E;
    syscallsMap["creat"] = 	PPME_SYSCALL_CREAT_E;
    syscallsMap["pipe"] = 	PPME_SYSCALL_PIPE_E;
    syscallsMap["eventfd"] = 	PPME_SYSCALL_EVENTFD_E;
    syscallsMap["futex"] = 	PPME_SYSCALL_FUTEX_E;
    syscallsMap["stat"] = 	PPME_SYSCALL_STAT_E;
    syscallsMap["lstat"] = 	PPME_SYSCALL_LSTAT_E;
    syscallsMap["fstat"] = 	PPME_SYSCALL_FSTAT_E;
    syscallsMap["stat64"] = 	PPME_SYSCALL_STAT64_E;
    syscallsMap["lstat64"] = 	PPME_SYSCALL_LSTAT64_E;
    syscallsMap["fstat64"] = 	PPME_SYSCALL_FSTAT64_E;
    syscallsMap["epollwait"] = 	PPME_SYSCALL_EPOLLWAIT_E;
    syscallsMap["poll"] = 	PPME_SYSCALL_POLL_E;
    syscallsMap["select"] = 	PPME_SYSCALL_SELECT_E;
    syscallsMap["newselect"] = 	PPME_SYSCALL_NEWSELECT_E;
    syscallsMap["lseek"] = 	PPME_SYSCALL_LSEEK_E;
    syscallsMap["llseek"] = 	PPME_SYSCALL_LLSEEK_E;
    syscallsMap["ioctl"] = 	PPME_SYSCALL_IOCTL_2_E;
    syscallsMap["getcwd"] = 	PPME_SYSCALL_GETCWD_E;
    syscallsMap["chdir"] = 	PPME_SYSCALL_CHDIR_E;
    syscallsMap["fchdir"] = 	PPME_SYSCALL_FCHDIR_E;
    syscallsMap["mkdir"] = 	PPME_SYSCALL_MKDIR_E;
    syscallsMap["rmdir"] = 	PPME_SYSCALL_RMDIR_E;
    syscallsMap["openat"] = 	PPME_SYSCALL_OPENAT_E;
    syscallsMap["link"] = 	PPME_SYSCALL_LINK_E;
    syscallsMap["linkat"] = 	PPME_SYSCALL_LINKAT_E;
    syscallsMap["unlink"] = 	PPME_SYSCALL_UNLINK_E;
    syscallsMap["unlinkat"] = 	PPME_SYSCALL_UNLINKAT_E;
    syscallsMap["pread"] = 	PPME_SYSCALL_PREAD_E;
    syscallsMap["pwrite"] = 	PPME_SYSCALL_PWRITE_E;
    syscallsMap["readv"] = 	PPME_SYSCALL_READV_E;
    syscallsMap["writev"] = 	PPME_SYSCALL_WRITEV_E;
    syscallsMap["preadv"] = 	PPME_SYSCALL_PREADV_E;
    syscallsMap["pwritev"] = 	PPME_SYSCALL_PWRITEV_E;
    syscallsMap["dup"] = 	PPME_SYSCALL_DUP_E;
    syscallsMap["signalfd"] = 	PPME_SYSCALL_SIGNALFD_E;
    syscallsMap["kill"] = 	PPME_SYSCALL_KILL_E;
    syscallsMap["tkill"] = 	PPME_SYSCALL_TKILL_E;
    syscallsMap["tgkill"] = 	PPME_SYSCALL_TGKILL_E;
    syscallsMap["nanosleep"] = 	PPME_SYSCALL_NANOSLEEP_E;
    syscallsMap["timerfd_create"] = 	PPME_SYSCALL_TIMERFD_CREATE_E;
    syscallsMap["getrlimit"] = 	PPME_SYSCALL_GETRLIMIT_E;
    syscallsMap["setrlimit"] = 	PPME_SYSCALL_SETRLIMIT_E;
    syscallsMap["prlimit"] = 	PPME_SYSCALL_PRLIMIT_E;
    syscallsMap["mmap"] = 	PPME_SYSCALL_MMAP_E;
    syscallsMap["munmap"] = 	PPME_SYSCALL_MUNMAP_E;
    syscallsMap["splice"] = 	PPME_SYSCALL_SPLICE_E;
    syscallsMap["ptrace"] = 	PPME_SYSCALL_PTRACE_E;
    syscallsMap["rename"] = 	PPME_SYSCALL_RENAME_E;
    syscallsMap["renameat"] = 	PPME_SYSCALL_RENAMEAT_E;
    syscallsMap["symlink"] = 	PPME_SYSCALL_SYMLINK_E;
    syscallsMap["symlinkat"] = 	PPME_SYSCALL_SYMLINKAT_E;
    syscallsMap["fork"] = 	PPME_SYSCALL_FORK_E;
    syscallsMap["vfork"] = 	PPME_SYSCALL_VFORK_E;
    syscallsMap["procexit"] = 	PPME_PROCEXIT_1_E;
    syscallsMap["sendfile"] = 	PPME_SYSCALL_SENDFILE_E;
    syscallsMap["setresuid"] = 	PPME_SYSCALL_SETRESUID_E;
    syscallsMap["setresgid"] = 	PPME_SYSCALL_SETRESGID_E;
    syscallsMap["setuid"] = 	PPME_SYSCALL_SETUID_E;
    syscallsMap["setgid"] = 	PPME_SYSCALL_SETGID_E;
    syscallsMap["signaldeliver"] = 	PPME_SIGNALDELIVER_E;
    syscallsMap["procinfo"] = 	PPME_PROCINFO_E;
    syscallsMap["setns"] = 	PPME_SYSCALL_SETNS_E;
    syscallsMap["semop"] = 	PPME_SYSCALL_SEMOP_E;
    syscallsMap["semctl"] = 	PPME_SYSCALL_SEMCTL_E;
    syscallsMap["ppoll"] = 	PPME_SYSCALL_PPOLL_E;
    syscallsMap["mount"] = 	PPME_SYSCALL_MOUNT_E;
    syscallsMap["umount"] = 	PPME_SYSCALL_UMOUNT_E;
    syscallsMap["chroot"] = 	PPME_SYSCALL_CHROOT_E;
}

SysdigService::~SysdigService(){
}

int
SysdigService::init(std::string chiselName, std::string brokerList, std::string format,
    bool useKafka, std::string defaultTopic, std::string networkTopic, int snapLen)
{
    return sysdigInitialize(chiselName, brokerList, format, useKafka, defaultTopic,
                            networkTopic, snapLen);
}

void
SysdigService::cleanup()
{
    using namespace std;

    sysdigCleanup();
}

bool
SysdigService::ready()
{
    using namespace std;

    return isSysdigInitialized();
}

void
SysdigService::runForever()
{
    using namespace std;

    while (!isSysdigInitialized()) {
        sleep(1);
    }

    sysdigStartProduction(terminate);
}

bool
SysdigService::stats(SysdigStats &out)
{
    using namespace std;

    sysdigDataT sysdigData;

    bool result = sysdigGetSysdigData(sysdigData);
    if (result) {
        out.nEvents = sysdigData.nEvents;
        out.nDrops = sysdigData.nDrops;
        out.nPreemptions = sysdigData.nPreemptions;
        out.nFilteredEvents = sysdigData.nFilteredEvents;
        out.nodeName = sysdigData.nodeName;
    }
    return result;
}

void
SysdigService::getSyscallIds(string syscall, std::vector<int>& ids) {
    ids.clear();
    if (syscallsMap.find(syscall) == syscallsMap.end())
        return;

    switch (syscallsMap[syscall]) {
        case PPME_SYSCALL_OPEN_E:
            ids.push_back(PPME_SYSCALL_OPEN_E);
            ids.push_back(PPME_SYSCALL_OPEN_X);
            break;
        case PPME_SYSCALL_CLOSE_E:
            ids.push_back(PPME_SYSCALL_CLOSE_E);
            ids.push_back(PPME_SYSCALL_CLOSE_X);
            break;
        case PPME_SYSCALL_READ_E:
            ids.push_back(PPME_SYSCALL_READ_E);
            ids.push_back(PPME_SYSCALL_READ_X);
            break;
        case PPME_SYSCALL_WRITE_E:
            ids.push_back(PPME_SYSCALL_WRITE_E);
            ids.push_back(PPME_SYSCALL_WRITE_X);
            break;
        case PPME_SYSCALL_BRK_1_E:
            ids.push_back(PPME_SYSCALL_BRK_1_E);
            ids.push_back(PPME_SYSCALL_BRK_1_X);
            ids.push_back(PPME_SYSCALL_BRK_4_E);
            ids.push_back(PPME_SYSCALL_BRK_4_X);
            break;
        case PPME_SYSCALL_EXECVE_8_E:
            ids.push_back(PPME_SYSCALL_EXECVE_8_E);
            ids.push_back(PPME_SYSCALL_EXECVE_8_X);
            ids.push_back(PPME_SYSCALL_EXECVE_13_E);
            ids.push_back(PPME_SYSCALL_EXECVE_13_X);
            ids.push_back(PPME_SYSCALL_EXECVE_14_E);
            ids.push_back(PPME_SYSCALL_EXECVE_14_X);
            ids.push_back(PPME_SYSCALL_EXECVE_15_E);
            ids.push_back(PPME_SYSCALL_EXECVE_15_X);
            ids.push_back(PPME_SYSCALL_EXECVE_16_E);
            ids.push_back(PPME_SYSCALL_EXECVE_16_X);
            break;
        case PPME_SYSCALL_CLONE_11_E:
            ids.push_back(PPME_SYSCALL_CLONE_11_E);
            ids.push_back(PPME_SYSCALL_CLONE_11_X);
            ids.push_back(PPME_SYSCALL_CLONE_16_E);
            ids.push_back(PPME_SYSCALL_CLONE_16_X);
            ids.push_back(PPME_SYSCALL_CLONE_17_E);
            ids.push_back(PPME_SYSCALL_CLONE_17_X);
            ids.push_back(PPME_SYSCALL_CLONE_20_E);
            ids.push_back(PPME_SYSCALL_CLONE_20_X);
            break;
        case PPME_PROCEXIT_E:
            ids.push_back(PPME_PROCEXIT_E);
            ids.push_back(PPME_PROCEXIT_X);
            break;
        case PPME_SOCKET_SOCKET_E:
            ids.push_back(PPME_SOCKET_SOCKET_E);
            ids.push_back(PPME_SOCKET_SOCKET_X);
            break;
        case PPME_SOCKET_BIND_E:
            ids.push_back(PPME_SOCKET_BIND_E);
            ids.push_back(PPME_SOCKET_BIND_X);
            break;
        case PPME_SOCKET_CONNECT_E:
            ids.push_back(PPME_SOCKET_CONNECT_E);
            ids.push_back(PPME_SOCKET_CONNECT_X);
            break;
        case PPME_SOCKET_LISTEN_E:
            ids.push_back(PPME_SOCKET_LISTEN_E);
            ids.push_back(PPME_SOCKET_LISTEN_X);
            break;
        case PPME_SOCKET_ACCEPT_E:
            ids.push_back(PPME_SOCKET_ACCEPT_E);
            ids.push_back(PPME_SOCKET_ACCEPT_X);
            ids.push_back(PPME_SOCKET_ACCEPT4_E);
            ids.push_back(PPME_SOCKET_ACCEPT4_X);
            ids.push_back(PPME_SOCKET_ACCEPT_5_E);
            ids.push_back(PPME_SOCKET_ACCEPT_5_X);
            ids.push_back(PPME_SOCKET_ACCEPT4_5_E);
            ids.push_back(PPME_SOCKET_ACCEPT4_5_X);
            break;
        case PPME_SOCKET_SEND_E:
            ids.push_back(PPME_SOCKET_SEND_E);
            ids.push_back(PPME_SOCKET_SEND_X);
            break;
        case PPME_SOCKET_SENDTO_E:
            ids.push_back(PPME_SOCKET_SENDTO_E);
            ids.push_back(PPME_SOCKET_SENDTO_X);
            break;
        case PPME_SOCKET_RECV_E:
            ids.push_back(PPME_SOCKET_RECV_E);
            ids.push_back(PPME_SOCKET_RECV_X);
            break;
        case PPME_SOCKET_RECVFROM_E:
            ids.push_back(PPME_SOCKET_RECVFROM_E);
            ids.push_back(PPME_SOCKET_RECVFROM_X);
            break;
        case PPME_SOCKET_SHUTDOWN_E:
            ids.push_back(PPME_SOCKET_SHUTDOWN_E);
            ids.push_back(PPME_SOCKET_SHUTDOWN_X);
            break;
        case PPME_SOCKET_GETSOCKNAME_E:
            ids.push_back(PPME_SOCKET_GETSOCKNAME_E);
            ids.push_back(PPME_SOCKET_GETSOCKNAME_X);
            break;
        case PPME_SOCKET_GETPEERNAME_E:
            ids.push_back(PPME_SOCKET_GETPEERNAME_E);
            ids.push_back(PPME_SOCKET_GETPEERNAME_X);
            break;
        case PPME_SOCKET_SOCKETPAIR_E:
            ids.push_back(PPME_SOCKET_SOCKETPAIR_E);
            ids.push_back(PPME_SOCKET_SOCKETPAIR_X);
            break;
        case PPME_SOCKET_SETSOCKOPT_E:
            ids.push_back(PPME_SOCKET_SETSOCKOPT_E);
            ids.push_back(PPME_SOCKET_SETSOCKOPT_X);
            break;
        case PPME_SOCKET_GETSOCKOPT_E:
            ids.push_back(PPME_SOCKET_GETSOCKOPT_E);
            ids.push_back(PPME_SOCKET_GETSOCKOPT_X);
            break;
        case PPME_SOCKET_SENDMSG_E:
            ids.push_back(PPME_SOCKET_SENDMSG_E);
            ids.push_back(PPME_SOCKET_SENDMSG_X);
            break;
        case PPME_SOCKET_SENDMMSG_E:
            ids.push_back(PPME_SOCKET_SENDMMSG_E);
            ids.push_back(PPME_SOCKET_SENDMMSG_X);
            break;
        case PPME_SOCKET_RECVMSG_E:
            ids.push_back(PPME_SOCKET_RECVMSG_E);
            ids.push_back(PPME_SOCKET_RECVMSG_X);
            break;
        case PPME_SOCKET_RECVMMSG_E:
            ids.push_back(PPME_SOCKET_RECVMMSG_E);
            ids.push_back(PPME_SOCKET_RECVMMSG_X);
            break;
        case PPME_SOCKET_ACCEPT4_E:
            ids.push_back(PPME_SOCKET_ACCEPT4_E);
            ids.push_back(PPME_SOCKET_ACCEPT4_X);
            break;
        case PPME_SYSCALL_CREAT_E:
            ids.push_back(PPME_SYSCALL_CREAT_E);
            ids.push_back(PPME_SYSCALL_CREAT_X);
            break;
        case PPME_SYSCALL_PIPE_E:
            ids.push_back(PPME_SYSCALL_PIPE_E);
            ids.push_back(PPME_SYSCALL_PIPE_X);
            break;
        case PPME_SYSCALL_EVENTFD_E:
            ids.push_back(PPME_SYSCALL_EVENTFD_E);
            ids.push_back(PPME_SYSCALL_EVENTFD_X);
            break;
        case PPME_SYSCALL_FUTEX_E:
            ids.push_back(PPME_SYSCALL_FUTEX_E);
            ids.push_back(PPME_SYSCALL_FUTEX_X);
            break;
        case PPME_SYSCALL_STAT_E:
            ids.push_back(PPME_SYSCALL_STAT_E);
            ids.push_back(PPME_SYSCALL_STAT_X);
            break;
        case PPME_SYSCALL_LSTAT_E:
            ids.push_back(PPME_SYSCALL_LSTAT_E);
            ids.push_back(PPME_SYSCALL_LSTAT_X);
            break;
        case PPME_SYSCALL_FSTAT_E:
            ids.push_back(PPME_SYSCALL_FSTAT_E);
            ids.push_back(PPME_SYSCALL_FSTAT_X);
            break;
        case PPME_SYSCALL_STAT64_E:
            ids.push_back(PPME_SYSCALL_STAT64_E);
            ids.push_back(PPME_SYSCALL_STAT64_X);
            break;
        case PPME_SYSCALL_LSTAT64_E:
            ids.push_back(PPME_SYSCALL_LSTAT64_E);
            ids.push_back(PPME_SYSCALL_LSTAT64_X);
            break;
        case PPME_SYSCALL_FSTAT64_E:
            ids.push_back(PPME_SYSCALL_FSTAT64_E);
            ids.push_back(PPME_SYSCALL_FSTAT64_X);
            break;
        case PPME_SYSCALL_EPOLLWAIT_E:
            ids.push_back(PPME_SYSCALL_EPOLLWAIT_E);
            ids.push_back(PPME_SYSCALL_EPOLLWAIT_X);
            break;
        case PPME_SYSCALL_POLL_E:
            ids.push_back(PPME_SYSCALL_POLL_E);
            ids.push_back(PPME_SYSCALL_POLL_X);
            break;
        case PPME_SYSCALL_SELECT_E:
            ids.push_back(PPME_SYSCALL_SELECT_E);
            ids.push_back(PPME_SYSCALL_SELECT_X);
            break;
        case PPME_SYSCALL_NEWSELECT_E:
            ids.push_back(PPME_SYSCALL_NEWSELECT_E);
            ids.push_back(PPME_SYSCALL_NEWSELECT_X);
            break;
        case PPME_SYSCALL_LSEEK_E:
            ids.push_back(PPME_SYSCALL_LSEEK_E);
            ids.push_back(PPME_SYSCALL_LSEEK_X);
            break;
        case PPME_SYSCALL_LLSEEK_E:
            ids.push_back(PPME_SYSCALL_LLSEEK_E);
            ids.push_back(PPME_SYSCALL_LLSEEK_X);
            break;
        case PPME_SYSCALL_IOCTL_2_E:
            ids.push_back(PPME_SYSCALL_IOCTL_2_E);
            ids.push_back(PPME_SYSCALL_IOCTL_2_X);
            ids.push_back(PPME_SYSCALL_IOCTL_3_E);
            ids.push_back(PPME_SYSCALL_IOCTL_3_X);
            break;
        case PPME_SYSCALL_GETCWD_E:
            ids.push_back(PPME_SYSCALL_GETCWD_E);
            ids.push_back(PPME_SYSCALL_GETCWD_X);
            break;
        case PPME_SYSCALL_CHDIR_E:
            ids.push_back(PPME_SYSCALL_CHDIR_E);
            ids.push_back(PPME_SYSCALL_CHDIR_X);
            break;
        case PPME_SYSCALL_FCHDIR_E:
            ids.push_back(PPME_SYSCALL_FCHDIR_E);
            ids.push_back(PPME_SYSCALL_FCHDIR_X);
            break;
        case PPME_SYSCALL_MKDIR_E:
            ids.push_back(PPME_SYSCALL_MKDIR_E);
            ids.push_back(PPME_SYSCALL_MKDIR_X);
            break;
        case PPME_SYSCALL_RMDIR_E:
            ids.push_back(PPME_SYSCALL_RMDIR_E);
            ids.push_back(PPME_SYSCALL_RMDIR_X);
            break;
        case PPME_SYSCALL_OPENAT_E:
            ids.push_back(PPME_SYSCALL_OPENAT_E);
            ids.push_back(PPME_SYSCALL_OPENAT_X);
            break;
        case PPME_SYSCALL_LINK_E:
            ids.push_back(PPME_SYSCALL_LINK_E);
            ids.push_back(PPME_SYSCALL_LINK_X);
            break;
        case PPME_SYSCALL_LINKAT_E:
            ids.push_back(PPME_SYSCALL_LINKAT_E);
            ids.push_back(PPME_SYSCALL_LINKAT_X);
            break;
        case PPME_SYSCALL_UNLINK_E:
            ids.push_back(PPME_SYSCALL_UNLINK_E);
            ids.push_back(PPME_SYSCALL_UNLINK_X);
            break;
        case PPME_SYSCALL_UNLINKAT_E:
            ids.push_back(PPME_SYSCALL_UNLINKAT_E);
            ids.push_back(PPME_SYSCALL_UNLINKAT_X);
            break;
        case PPME_SYSCALL_PREAD_E:
            ids.push_back(PPME_SYSCALL_PREAD_E);
            ids.push_back(PPME_SYSCALL_PREAD_X);
            break;
        case PPME_SYSCALL_PWRITE_E:
            ids.push_back(PPME_SYSCALL_PWRITE_E);
            ids.push_back(PPME_SYSCALL_PWRITE_X);
            break;
        case PPME_SYSCALL_READV_E:
            ids.push_back(PPME_SYSCALL_READV_E);
            ids.push_back(PPME_SYSCALL_READV_X);
            break;
        case PPME_SYSCALL_WRITEV_E:
            ids.push_back(PPME_SYSCALL_WRITEV_E);
            ids.push_back(PPME_SYSCALL_WRITEV_X);
            break;
        case PPME_SYSCALL_PREADV_E:
            ids.push_back(PPME_SYSCALL_PREADV_E);
            ids.push_back(PPME_SYSCALL_PREADV_X);
            break;
        case PPME_SYSCALL_PWRITEV_E:
            ids.push_back(PPME_SYSCALL_PWRITEV_E);
            ids.push_back(PPME_SYSCALL_PWRITEV_X);
            break;
        case PPME_SYSCALL_DUP_E:
            ids.push_back(PPME_SYSCALL_DUP_E);
            ids.push_back(PPME_SYSCALL_DUP_X);
            break;
        case PPME_SYSCALL_SIGNALFD_E:
            ids.push_back(PPME_SYSCALL_SIGNALFD_E);
            ids.push_back(PPME_SYSCALL_SIGNALFD_X);
            break;
        case PPME_SYSCALL_KILL_E:
            ids.push_back(PPME_SYSCALL_KILL_E);
            ids.push_back(PPME_SYSCALL_KILL_X);
            break;
        case PPME_SYSCALL_TKILL_E:
            ids.push_back(PPME_SYSCALL_TKILL_E);
            ids.push_back(PPME_SYSCALL_TKILL_X);
            break;
        case PPME_SYSCALL_TGKILL_E:
            ids.push_back(PPME_SYSCALL_TGKILL_E);
            ids.push_back(PPME_SYSCALL_TGKILL_X);
            break;
        case PPME_SYSCALL_NANOSLEEP_E:
            ids.push_back(PPME_SYSCALL_NANOSLEEP_E);
            ids.push_back(PPME_SYSCALL_NANOSLEEP_X);
            break;
        case PPME_SYSCALL_TIMERFD_CREATE_E:
            ids.push_back(PPME_SYSCALL_TIMERFD_CREATE_E);
            ids.push_back(PPME_SYSCALL_TIMERFD_CREATE_X);
            break;
        case PPME_SYSCALL_GETRLIMIT_E:
            ids.push_back(PPME_SYSCALL_GETRLIMIT_E);
            ids.push_back(PPME_SYSCALL_GETRLIMIT_X);
            break;
        case PPME_SYSCALL_SETRLIMIT_E:
            ids.push_back(PPME_SYSCALL_SETRLIMIT_E);
            ids.push_back(PPME_SYSCALL_SETRLIMIT_X);
            break;
        case PPME_SYSCALL_PRLIMIT_E:
            ids.push_back(PPME_SYSCALL_PRLIMIT_E);
            ids.push_back(PPME_SYSCALL_PRLIMIT_X);
            break;
        case PPME_SYSCALL_MMAP_E:
            ids.push_back(PPME_SYSCALL_MMAP_E);
            ids.push_back(PPME_SYSCALL_MMAP_X);
            ids.push_back(PPME_SYSCALL_MMAP2_E);
            ids.push_back(PPME_SYSCALL_MMAP2_X);
            break;
        case PPME_SYSCALL_MUNMAP_E:
            ids.push_back(PPME_SYSCALL_MUNMAP_E);
            ids.push_back(PPME_SYSCALL_MUNMAP_X);
            break;
        case PPME_SYSCALL_SPLICE_E:
            ids.push_back(PPME_SYSCALL_SPLICE_E);
            ids.push_back(PPME_SYSCALL_SPLICE_X);
            break;
        case PPME_SYSCALL_PTRACE_E:
            ids.push_back(PPME_SYSCALL_PTRACE_E);
            ids.push_back(PPME_SYSCALL_PTRACE_X);
            break;
        case PPME_SYSCALL_RENAME_E:
            ids.push_back(PPME_SYSCALL_RENAME_E);
            ids.push_back(PPME_SYSCALL_RENAME_X);
            break;
        case PPME_SYSCALL_RENAMEAT_E:
            ids.push_back(PPME_SYSCALL_RENAMEAT_E);
            ids.push_back(PPME_SYSCALL_RENAMEAT_X);
            break;
        case PPME_SYSCALL_SYMLINK_E:
            ids.push_back(PPME_SYSCALL_SYMLINK_E);
            ids.push_back(PPME_SYSCALL_SYMLINK_X);
            break;
        case PPME_SYSCALL_SYMLINKAT_E:
            ids.push_back(PPME_SYSCALL_SYMLINKAT_E);
            ids.push_back(PPME_SYSCALL_SYMLINKAT_X);
            break;
        case PPME_SYSCALL_FORK_E:
            ids.push_back(PPME_SYSCALL_FORK_E);
            ids.push_back(PPME_SYSCALL_FORK_X);
            ids.push_back(PPME_SYSCALL_FORK_17_E);
            ids.push_back(PPME_SYSCALL_FORK_17_X);
            ids.push_back(PPME_SYSCALL_FORK_20_E);
            ids.push_back(PPME_SYSCALL_FORK_20_X);
            break;
        case PPME_SYSCALL_VFORK_E:
            ids.push_back(PPME_SYSCALL_VFORK_E);
            ids.push_back(PPME_SYSCALL_VFORK_X);
            ids.push_back(PPME_SYSCALL_VFORK_17_E);
            ids.push_back(PPME_SYSCALL_VFORK_17_X);
            ids.push_back(PPME_SYSCALL_VFORK_20_E);
            ids.push_back(PPME_SYSCALL_VFORK_20_X);
            break;
        case PPME_PROCEXIT_1_E:
            ids.push_back(PPME_PROCEXIT_1_E);
            ids.push_back(PPME_PROCEXIT_1_X);
            break;
        case PPME_SYSCALL_SENDFILE_E:
            ids.push_back(PPME_SYSCALL_SENDFILE_E);
            ids.push_back(PPME_SYSCALL_SENDFILE_X);
            break;
        case PPME_SYSCALL_SETRESUID_E:
            ids.push_back(PPME_SYSCALL_SETRESUID_E);
            ids.push_back(PPME_SYSCALL_SETRESUID_X);
            break;
        case PPME_SYSCALL_SETRESGID_E:
            ids.push_back(PPME_SYSCALL_SETRESGID_E);
            ids.push_back(PPME_SYSCALL_SETRESGID_X);
            break;
        case PPME_SYSCALL_SETUID_E:
            ids.push_back(PPME_SYSCALL_SETUID_E);
            ids.push_back(PPME_SYSCALL_SETUID_X);
            break;
        case PPME_SYSCALL_SETGID_E:
            ids.push_back(PPME_SYSCALL_SETGID_E);
            ids.push_back(PPME_SYSCALL_SETGID_X);
            break;
        case PPME_SIGNALDELIVER_E:
            ids.push_back(PPME_SIGNALDELIVER_E);
            ids.push_back(PPME_SIGNALDELIVER_X);
            break;
        case PPME_PROCINFO_E:
            ids.push_back(PPME_PROCINFO_E);
            ids.push_back(PPME_PROCINFO_X);
            break;
        case PPME_SYSCALL_SETNS_E:
            ids.push_back(PPME_SYSCALL_SETNS_E);
            ids.push_back(PPME_SYSCALL_SETNS_X);
            break;
        case PPME_SYSCALL_SEMOP_E:
            ids.push_back(PPME_SYSCALL_SEMOP_E);
            ids.push_back(PPME_SYSCALL_SEMOP_X);
            break;
        case PPME_SYSCALL_SEMCTL_E:
            ids.push_back(PPME_SYSCALL_SEMCTL_E);
            ids.push_back(PPME_SYSCALL_SEMCTL_X);
            break;
        case PPME_SYSCALL_PPOLL_E:
            ids.push_back(PPME_SYSCALL_PPOLL_E);
            ids.push_back(PPME_SYSCALL_PPOLL_X);
            break;
        case PPME_SYSCALL_MOUNT_E:
            ids.push_back(PPME_SYSCALL_MOUNT_E);
            ids.push_back(PPME_SYSCALL_MOUNT_X);
            break;
        case PPME_SYSCALL_UMOUNT_E:
            ids.push_back(PPME_SYSCALL_UMOUNT_E);
            ids.push_back(PPME_SYSCALL_UMOUNT_X);
            break;
        case PPME_SYSCALL_CHROOT_E:
            ids.push_back(PPME_SYSCALL_CHROOT_E);
            ids.push_back(PPME_SYSCALL_CHROOT_X);
            break;
    }
}

}   /* namespace collector */
