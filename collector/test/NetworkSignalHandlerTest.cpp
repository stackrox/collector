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

TEST(ProcessSignalFormatterTest, ProcessWithParentTest) {
  std::unique_ptr<sinsp> inspector(new_inspector());
  CollectorStats& collector_stats = CollectorStats::GetOrCreate();

  ProcessSignalFormatter processSignalFormatter(inspector.get());

  auto tinfo = std::make_shared<sinsp_threadinfo>(inspector.get());
  tinfo->m_pid = 3;
  tinfo->m_tid = 3;
  tinfo->m_ptid = -1;
  tinfo->m_vpid = 1;
  tinfo->m_user.uid = 42;
  tinfo->m_exepath = "asdf";
  auto tinfo2 = std::make_shared<sinsp_threadinfo>(inspector.get());
  tinfo2->m_pid = 1;
  tinfo2->m_tid = 1;
  tinfo2->m_ptid = 3;
  tinfo2->m_vpid = 2;
  tinfo2->m_user.uid = 7;
  tinfo2->m_exepath = "qwerty";
  inspector->add_thread(tinfo);
  inspector->add_thread(tinfo2);
  std::vector<ProcessSignalFormatter::LineageInfo> lineage;
  processSignalFormatter.GetProcessLineage(tinfo2.get(), lineage);

  int count = collector_stats.GetCounter(CollectorStats::process_lineage_counts);
  int total = collector_stats.GetCounter(CollectorStats::process_lineage_total);
  int sqrTotal = collector_stats.GetCounter(CollectorStats::process_lineage_sqr_total);
  int stringTotal = collector_stats.GetCounter(CollectorStats::process_lineage_string_total);

  EXPECT_EQ(count, 1);
  EXPECT_EQ(total, 1);
  EXPECT_EQ(sqrTotal, 1);
  EXPECT_EQ(stringTotal, 4);

  EXPECT_EQ(lineage.size(), 1);

  EXPECT_EQ(lineage[0].parent_uid(), tinfo->m_user.uid);
  EXPECT_EQ(lineage[0].parent_exec_file_path(), tinfo->m_exepath);

  CollectorStats::Reset();
}

TEST(NetworkSignalHandlerTest, NullEventTest) {
  std::unique_ptr<sinsp> inspector(new_inspector());
  std::shared_ptr<ConnectionTracker> conn_tracker;
  SysdigStats* stats;
  sinsp_evt* evt = nullptr;
  std::optional<Connection> conn;

  NetworkSignalHandler networkSignalHandler = NetworkSignalHandler(inspector.get(), conn_tracker, stats);
  conn = networkSignalHandler.GetConnection(evt);

  EXPECT_EQ(conn, std::nullopt);
}

TEST(NetworkSignalHandlerTest, NoFdInfoTest) {
  std::unique_ptr<sinsp> inspector(new_inspector());
  std::shared_ptr<ConnectionTracker> conn_tracker;
  SysdigStats* stats;
  sinsp_evt* evt = new sinsp_evt(inspector.get());
  std::optional<Connection> conn;

  scap_evt* scap_event = new scap_evt;
  ppm_event_info* ppm_event = new ppm_event_info;
  sinsp_fdinfo_t* fdinfo = nullptr;

  scap_event->ts = 0;
  scap_event->tid = 3;
  scap_event->len = 0;
  scap_event->type = 0;
  scap_event->nparams = 0;

  ppm_event->flags = EF_NONE;

  sinsp_threadinfo* threadinfo = new sinsp_threadinfo(inspector.get());
  threadinfo->m_pid = 3;
  threadinfo->m_tid = 3;
  threadinfo->m_ptid = -1;
  threadinfo->m_vpid = 1;
  threadinfo->m_user.uid = 42;
  threadinfo->m_exepath = "asdf";

  evt->init(scap_event, ppm_event, threadinfo, fdinfo);

  NetworkSignalHandler networkSignalHandler = NetworkSignalHandler(inspector.get(), conn_tracker, stats);
  conn = networkSignalHandler.GetConnection(evt);

  EXPECT_EQ(conn, std::nullopt);
}

}  // namespace

}  // namespace collector
