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

// clang-format off
// sinsp.h needs to be included before chisel.h
#include <Utility.h>
#include "common/strlcpy.h"
#include "libsinsp/sinsp.h"
#include "libsinsp/parsers.h"
#include "chisel.h"
#include "libsinsp/wrapper.h"
#include "libsinsp/test/sinsp_with_test_input.h"
#include "libsinsp/test/test_utils.h"
// clang-format on

#include "NetworkSignalHandlerTest.h"

#include <cstring>
#include <stdint.h>

#include <arpa/inet.h>
#include <linux/un.h>

#include "CollectorStats.h"
#include "NetworkSignalHandler.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

//#include "libscap/userspace_flag_helpers.h"

namespace collector {

// using ProcessSignal = ProcessSignalFormatter::ProcessSignal;
// using LineageInfo = ProcessSignalFormatter::LineageInfo;

namespace {

TEST_F(sinsp_with_test_input, FdInfoTest) {
  add_default_init_thread();
  open_inspector();
  sinsp_evt* evt = NULL;

  sockaddr_in src, dest;
  dest.sin_family = AF_INET;
  dest.sin_port = htons(443);
  inet_aton("142.251.111.147", &dest.sin_addr);

  src.sin_family = AF_INET;
  src.sin_port = htons(54321);
  inet_aton("172.40.111.222", &src.sin_addr);

  std::vector<uint8_t> dest_sockaddr = test_utils::pack_sockaddr(reinterpret_cast<sockaddr*>(&dest));

  add_event_advance_ts(increasing_ts(), 1, PPME_SOCKET_SOCKET_E, 3, PPM_AF_INET, 0x80002, 0);
  evt = add_event_advance_ts(increasing_ts(), 1, PPME_SOCKET_SOCKET_X, 1, 7);
  ASSERT_EQ(get_field_as_string(evt, "fd.connected"), "false");

  add_event_advance_ts(increasing_ts(), 1, PPME_SOCKET_CONNECT_E, 2, 7, scap_const_sized_buffer{dest_sockaddr.data(), dest_sockaddr.size()});
  std::vector<uint8_t> socktuple = test_utils::pack_socktuple(reinterpret_cast<sockaddr*>(&src), reinterpret_cast<sockaddr*>(&dest));
  evt = add_event_advance_ts(increasing_ts(), 1, PPME_SOCKET_CONNECT_X, 2, 0, scap_const_sized_buffer{socktuple.data(), socktuple.size()});
  ASSERT_EQ(get_field_as_string(evt, "fd.name"), "172.40.111.222:54321->142.251.111.147:443");
  ASSERT_EQ(get_field_as_string(evt, "fd.connected"), "true");

  EXPECT_EQ(true, true);
}

// TEST(ProcessSignalFormatterTest, ProcessWithParentTest) {
//   std::unique_ptr<sinsp> inspector(new_inspector());
//   CollectorStats& collector_stats = CollectorStats::GetOrCreate();
//
//   ProcessSignalFormatter processSignalFormatter(inspector.get());
//
//   auto tinfo = std::make_shared<sinsp_threadinfo>(inspector.get());
//   tinfo->m_pid = 3;
//   tinfo->m_tid = 3;
//   tinfo->m_ptid = -1;
//   tinfo->m_vpid = 1;
//   tinfo->m_user.uid = 42;
//   tinfo->m_exepath = "asdf";
//   auto tinfo2 = std::make_shared<sinsp_threadinfo>(inspector.get());
//   tinfo2->m_pid = 1;
//   tinfo2->m_tid = 1;
//   tinfo2->m_ptid = 3;
//   tinfo2->m_vpid = 2;
//   tinfo2->m_user.uid = 7;
//   tinfo2->m_exepath = "qwerty";
//   inspector->add_thread(tinfo);
//   inspector->add_thread(tinfo2);
//   std::vector<ProcessSignalFormatter::LineageInfo> lineage;
//   processSignalFormatter.GetProcessLineage(tinfo2.get(), lineage);
//
//   int count = collector_stats.GetCounter(CollectorStats::process_lineage_counts);
//   int total = collector_stats.GetCounter(CollectorStats::process_lineage_total);
//   int sqrTotal = collector_stats.GetCounter(CollectorStats::process_lineage_sqr_total);
//   int stringTotal = collector_stats.GetCounter(CollectorStats::process_lineage_string_total);
//
//   EXPECT_EQ(count, 1);
//   EXPECT_EQ(total, 1);
//   EXPECT_EQ(sqrTotal, 1);
//   EXPECT_EQ(stringTotal, 4);
//
//   EXPECT_EQ(lineage.size(), 1);
//
//   EXPECT_EQ(lineage[0].parent_uid(), tinfo->m_user.uid);
//   EXPECT_EQ(lineage[0].parent_exec_file_path(), tinfo->m_exepath);
//
//   CollectorStats::Reset();
// }

// TEST(NetworkSignalHandlerTest, NullEventTest) {
//   std::unique_ptr<sinsp> inspector(new_inspector());
//   std::shared_ptr<ConnectionTracker> conn_tracker;
//   SysdigStats* stats;
//   sinsp_evt* evt = nullptr;
//   std::optional<Connection> conn;
//
//   NetworkSignalHandler networkSignalHandler = NetworkSignalHandler(inspector.get(), conn_tracker, stats);
//   conn = networkSignalHandler.GetConnection(evt);
//
//   EXPECT_EQ(conn, std::nullopt);
// }
//
// TEST(NetworkSignalHandlerTest, NoFdInfoTest) {
//   std::unique_ptr<sinsp> inspector(new_inspector());
//   std::shared_ptr<ConnectionTracker> conn_tracker;
//   SysdigStats* stats;
//   sinsp_evt* evt = new sinsp_evt(inspector.get());
//   std::optional<Connection> conn;
//
//   scap_evt* scap_event = new scap_evt;
//   ppm_event_info* ppm_event = new ppm_event_info;
//   sinsp_fdinfo_t* fdinfo = nullptr;
//
//   scap_event->ts = 0;
//   scap_event->tid = 3;
//   scap_event->len = 0;
//   scap_event->type = 0;
//   scap_event->nparams = 0;
//
//   ppm_event->flags = EF_NONE;
//
//   sinsp_threadinfo* threadinfo = new sinsp_threadinfo(inspector.get());
//   threadinfo->m_pid = 3;
//   threadinfo->m_tid = 3;
//   threadinfo->m_ptid = -1;
//   threadinfo->m_vpid = 1;
//   threadinfo->m_user.uid = 42;
//   threadinfo->m_exepath = "asdf";
//
//   evt->init(scap_event, ppm_event, threadinfo, fdinfo);
//
//   NetworkSignalHandler networkSignalHandler = NetworkSignalHandler(inspector.get(), conn_tracker, stats);
//   conn = networkSignalHandler.GetConnection(evt);
//
//   EXPECT_EQ(conn, std::nullopt);
// }

// TEST(NetworkSignalHandlerTest, FdInfoTest) {
//   std::unique_ptr<sinsp> inspector(new_inspector());
//   std::shared_ptr<ConnectionTracker> conn_tracker;
//   SysdigStats* stats;
//   sinsp_evt *evt = new sinsp_evt(inspector.get());
//   std::optional<Connection> conn;
//
//   scap_evt *scap_event = new scap_evt;
//   ppm_event_info *ppm_event = new ppm_event_info;
//   sinsp_fdinfo_t *fdinfo = new sinsp_fdinfo_t;
//
//   scap_event->ts = 0;
//   scap_event->tid = 3;
//   scap_event->len = 0;
//   scap_event->type = PPME_CONTAINER_JSON_E;
//   scap_event->nparams = 0;
//
//   ppm_event->flags = EF_USES_FD;
//
//   sinsp_threadinfo *threadinfo = new sinsp_threadinfo(inspector.get());
//   threadinfo->m_pid = 3;
//   threadinfo->m_tid = 3;
//   threadinfo->m_ptid = -1;
//   threadinfo->m_vpid = 1;
//   threadinfo->m_user.uid = 42;
//   threadinfo->m_exepath = "asdf";
//
//   //fdinfo->set_role_server();
//   fdinfo->m_type = SCAP_FD_IPV4_SOCK;
//   sinsp_sockinfo sockinfo;
//   sockinfo.m_ipv4info.m_fields.m_sip = 0;
//   fdinfo->m_sockinfo = sockinfo;
//
//   evt->init(scap_event, ppm_event, threadinfo, fdinfo);
//   sinsp_parser *parser = new sinsp_parser(inspector.get());
//   //parser->process_event(evt);
//
//   NetworkSignalHandler networkSignalHandler = NetworkSignalHandler(inspector.get(), conn_tracker, stats);
//   conn = networkSignalHandler.GetConnection(evt);
//
//   EXPECT_EQ(conn, std::nullopt);
// }
//

// inline void vecbuf_append(std::vector<uint8_t> &dest, void* src, size_t size)
//{
//         uint8_t *src_bytes = reinterpret_cast<uint8_t*>(src);
//         for (size_t i = 0; i < size; i++) {
//                 uint8_t byte;
//                 memcpy(&byte, src_bytes + i, 1);
//                 dest.push_back(byte);
//         }
// }
//
// std::vector<uint8_t> pack_addr(sockaddr *sa)
//{
//         std::vector<uint8_t> res;
//         switch(sa->sa_family)
//         {
//                 case AF_INET:
//                 {
//                         sockaddr_in *sa_in = (sockaddr_in *)sa;
//                         vecbuf_append(res, &sa_in->sin_addr.s_addr, sizeof(sa_in->sin_addr.s_addr));
//                 }
//                 break;
//
//                 case AF_INET6:
//                 {
//                         sockaddr_in6 *sa_in6 = (sockaddr_in6 *)sa;
//                         vecbuf_append(res, &sa_in6->sin6_addr, 2 * sizeof(uint64_t));
//                 }
//                 break;
//
//                 case AF_UNIX:
//                 {
//                         sockaddr_un *sa_un = (sockaddr_un *)sa;
//                         std::string path = std::string(sa_un->sun_path);
//                         path = path.substr(0, UNIX_PATH_MAX);
//                         path.push_back('\0');
//                         res.insert(res.end(), path.begin(), path.end());
//                 }
//                 break;
//         }
//
//         return res;
// }
//
// uint8_t get_sock_family(sockaddr *sa)
//{
//         uint8_t sock_family = 0;
//         switch(sa->sa_family)
//         {
//                 case AF_INET:
//                 {
//                         sockaddr_in *sa_in = (sockaddr_in *)sa;
//                         sock_family = socket_family_to_scap(sa_in->sin_family);
//                 }
//                 break;
//
//                 case AF_INET6:
//                 {
//                         sockaddr_in6 *sa_in6 = (sockaddr_in6 *)sa;
//                         sock_family = socket_family_to_scap(sa_in6->sin6_family);
//                 }
//                 break;
//
//                 case AF_UNIX:
//                 {
//                         sockaddr_un *sa_un = (sockaddr_un *)sa;
//                         sock_family = socket_family_to_scap(sa_un->sun_family);
//                 }
//                 break;
//         }
//
//         return sock_family;
// }
//
// std::vector<uint8_t> pack_sockaddr(sockaddr *sa)
//{
//         std::vector<uint8_t> res;
//         res.push_back(get_sock_family(sa));
//         auto addr_port = pack_addr_port(sa);
//         res.insert(res.end(), addr_port.begin(), addr_port.end());
//
//         return res;
// }
//
// std::vector<uint8_t> pack_socktuple(sockaddr *src, sockaddr *dest)
//{
//         std::vector<uint8_t> res;
//
//         res.push_back(get_sock_family(src));
//         auto src_addr = pack_addr_port(src);
//         auto dest_addr = pack_addr_port(dest);
//
//         res.insert(res.end(), src_addr.begin(), src_addr.end());
//         res.insert(res.end(), dest_addr.begin(), dest_addr.end());
//
//         return res;
// }

}  // namespace

}  // namespace collector
