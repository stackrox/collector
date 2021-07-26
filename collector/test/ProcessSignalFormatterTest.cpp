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
#include "libsinsp/sinsp.h"
#include "libsinsp/chisel.h"
#include "libsinsp/wrapper.h"
// clang-format on

#include "CollectorStats.h"
#include "ProcessSignalFormatter.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace collector {

using ProcessSignal = ProcessSignalFormatter::ProcessSignal;
using LineageInfo = ProcessSignalFormatter::LineageInfo;

namespace {

TEST(ProcessSignalFormatterTest, NoProcessTest) {
  sinsp* inspector = NULL;
  CollectorStats* collector_stats = CollectorStats::GetOrCreate();

  ProcessSignalFormatter processSignalFormatter(inspector);

  sinsp_threadinfo* tinfo = NULL;
  std::vector<LineageInfo> lineage;

  processSignalFormatter.GetProcessLineage(tinfo, lineage);

  int count = collector_stats->GetCounter(CollectorStats::process_lineage_counts);
  int total = collector_stats->GetCounter(CollectorStats::process_lineage_total);
  int sqrTotal = collector_stats->GetCounter(CollectorStats::process_lineage_sqr_total);
  int stringTotal = collector_stats->GetCounter(CollectorStats::process_lineage_string_total);

  EXPECT_EQ(count, 0);
  EXPECT_EQ(total, 0);
  EXPECT_EQ(sqrTotal, 0);
  EXPECT_EQ(stringTotal, 0);

  EXPECT_TRUE(lineage.empty());
}

TEST(ProcessSignalFormatterTest, ProcessWithoutParentTest) {
  sinsp* inspector = new_inspector();
  CollectorStats* collector_stats = CollectorStats::GetOrCreate();

  ProcessSignalFormatter processSignalFormatter(inspector);

  auto tinfo = std::make_shared<sinsp_threadinfo>(inspector);
  tinfo->m_pid = 0;
  tinfo->m_tid = 0;

  inspector->add_thread(tinfo);
  std::vector<LineageInfo> lineage;

  processSignalFormatter.GetProcessLineage(tinfo.get(), lineage);

  int count = collector_stats->GetCounter(CollectorStats::process_lineage_counts);
  int total = collector_stats->GetCounter(CollectorStats::process_lineage_total);
  int sqrTotal = collector_stats->GetCounter(CollectorStats::process_lineage_sqr_total);
  int stringTotal = collector_stats->GetCounter(CollectorStats::process_lineage_string_total);

  EXPECT_EQ(count, 1);
  EXPECT_EQ(total, 0);
  EXPECT_EQ(sqrTotal, 0);
  EXPECT_EQ(stringTotal, 0);

  EXPECT_TRUE(lineage.empty());

  CollectorStats::Reset();
}

TEST(ProcessSignalFormatterTest, ProcessWithParentTest) {
  sinsp* inspector = new_inspector();
  CollectorStats* collector_stats = CollectorStats::GetOrCreate();

  ProcessSignalFormatter processSignalFormatter(inspector);

  auto tinfo = std::make_shared<sinsp_threadinfo>(inspector);
  tinfo->m_pid = 3;
  tinfo->m_tid = 3;
  tinfo->m_vpid = 1;
  tinfo->m_uid = 42;
  tinfo->m_exepath = "asdf";
  auto tinfo2 = std::make_shared<sinsp_threadinfo>(inspector);
  tinfo2->m_pid = 1;
  tinfo2->m_tid = 1;
  tinfo2->m_ptid = 3;
  tinfo2->m_vpid = 2;
  tinfo2->m_uid = 7;
  tinfo2->m_exepath = "qwerty";
  inspector->add_thread(tinfo);
  inspector->add_thread(tinfo2);
  std::vector<ProcessSignalFormatter::LineageInfo> lineage;
  processSignalFormatter.GetProcessLineage(tinfo2.get(), lineage);

  int count = collector_stats->GetCounter(CollectorStats::process_lineage_counts);
  int total = collector_stats->GetCounter(CollectorStats::process_lineage_total);
  int sqrTotal = collector_stats->GetCounter(CollectorStats::process_lineage_sqr_total);
  int stringTotal = collector_stats->GetCounter(CollectorStats::process_lineage_string_total);

  EXPECT_EQ(count, 1);
  EXPECT_EQ(total, 1);
  EXPECT_EQ(sqrTotal, 1);
  EXPECT_EQ(stringTotal, 4);

  EXPECT_EQ(lineage.size(), 1);

  EXPECT_EQ(lineage[0].parent_uid(), tinfo->m_uid);
  EXPECT_EQ(lineage[0].parent_exec_file_path(), tinfo->m_exepath);

  CollectorStats::Reset();
}

TEST(ProcessSignalFormatterTest, ProcessWithParentWithPid0Test) {
  sinsp* inspector = new_inspector();
  CollectorStats* collector_stats = CollectorStats::GetOrCreate();

  ProcessSignalFormatter processSignalFormatter(inspector);

  auto tinfo = std::make_shared<sinsp_threadinfo>(inspector);
  tinfo->m_pid = 0;
  tinfo->m_tid = 0;
  tinfo->m_vpid = 1;
  tinfo->m_exepath = "asdf";
  auto tinfo2 = std::make_shared<sinsp_threadinfo>(inspector);
  tinfo2->m_pid = 1;
  tinfo2->m_tid = 1;
  tinfo2->m_ptid = 0;
  tinfo2->m_vpid = 2;
  tinfo2->m_exepath = "qwerty";
  inspector->add_thread(tinfo);
  inspector->add_thread(tinfo2);
  std::vector<ProcessSignalFormatter::LineageInfo> lineage;
  processSignalFormatter.GetProcessLineage(tinfo2.get(), lineage);

  int count = collector_stats->GetCounter(CollectorStats::process_lineage_counts);
  int total = collector_stats->GetCounter(CollectorStats::process_lineage_total);
  int sqrTotal = collector_stats->GetCounter(CollectorStats::process_lineage_sqr_total);
  int stringTotal = collector_stats->GetCounter(CollectorStats::process_lineage_string_total);

  EXPECT_EQ(count, 1);
  EXPECT_EQ(total, 0);
  EXPECT_EQ(sqrTotal, 0);
  EXPECT_EQ(stringTotal, 0);

  EXPECT_TRUE(lineage.empty());

  CollectorStats::Reset();
}

TEST(ProcessSignalFormatterTest, ProcessWithParentWithSameNameTest) {
  sinsp* inspector = new_inspector();
  CollectorStats* collector_stats = CollectorStats::GetOrCreate();

  ProcessSignalFormatter processSignalFormatter(inspector);

  auto tinfo = std::make_shared<sinsp_threadinfo>(inspector);
  tinfo->m_pid = 3;
  tinfo->m_tid = 3;
  tinfo->m_vpid = 1;
  tinfo->m_uid = 43;
  tinfo->m_exepath = "asdf";
  auto tinfo2 = std::make_shared<sinsp_threadinfo>(inspector);
  tinfo2->m_pid = 1;
  tinfo2->m_tid = 1;
  tinfo2->m_ptid = 3;
  tinfo2->m_vpid = 2;
  tinfo2->m_uid = 42;
  tinfo2->m_exepath = "asdf";
  inspector->add_thread(tinfo);
  inspector->add_thread(tinfo2);
  std::vector<ProcessSignalFormatter::LineageInfo> lineage;
  processSignalFormatter.GetProcessLineage(tinfo2.get(), lineage);

  int count = collector_stats->GetCounter(CollectorStats::process_lineage_counts);
  int total = collector_stats->GetCounter(CollectorStats::process_lineage_total);
  int sqrTotal = collector_stats->GetCounter(CollectorStats::process_lineage_sqr_total);
  int stringTotal = collector_stats->GetCounter(CollectorStats::process_lineage_string_total);

  EXPECT_EQ(count, 1);
  EXPECT_EQ(total, 1);
  EXPECT_EQ(sqrTotal, 1);
  EXPECT_EQ(stringTotal, 4);

  EXPECT_EQ(lineage.size(), 1);

  EXPECT_EQ(lineage[0].parent_uid(), tinfo->m_uid);
  EXPECT_EQ(lineage[0].parent_exec_file_path(), tinfo->m_exepath);

  CollectorStats::Reset();
}

TEST(ProcessSignalFormatterTest, ProcessWithTwoParentsTest) {
  sinsp* inspector = new_inspector();
  CollectorStats* collector_stats = CollectorStats::GetOrCreate();

  ProcessSignalFormatter processSignalFormatter(inspector);

  auto tinfo = std::make_shared<sinsp_threadinfo>(inspector);
  tinfo->m_pid = 3;
  tinfo->m_tid = 3;
  tinfo->m_vpid = 1;
  tinfo->m_uid = 42;
  tinfo->m_exepath = "asdf";

  auto tinfo2 = std::make_shared<sinsp_threadinfo>(inspector);
  tinfo2->m_pid = 1;
  tinfo2->m_tid = 1;
  tinfo2->m_ptid = 3;
  tinfo2->m_vpid = 2;
  tinfo2->m_uid = 7;
  tinfo2->m_exepath = "qwerty";

  auto tinfo3 = std::make_shared<sinsp_threadinfo>(inspector);
  tinfo3->m_pid = 4;
  tinfo3->m_tid = 4;
  tinfo3->m_ptid = 1;
  tinfo3->m_vpid = 9;
  tinfo3->m_uid = 8;
  tinfo3->m_exepath = "uiop";

  inspector->add_thread(tinfo);
  inspector->add_thread(tinfo2);
  inspector->add_thread(tinfo3);

  std::vector<ProcessSignalFormatter::LineageInfo> lineage;
  processSignalFormatter.GetProcessLineage(tinfo3.get(), lineage);

  int count = collector_stats->GetCounter(CollectorStats::process_lineage_counts);
  int total = collector_stats->GetCounter(CollectorStats::process_lineage_total);
  int sqrTotal = collector_stats->GetCounter(CollectorStats::process_lineage_sqr_total);
  int stringTotal = collector_stats->GetCounter(CollectorStats::process_lineage_string_total);

  EXPECT_EQ(count, 1);
  EXPECT_EQ(total, 2);
  EXPECT_EQ(sqrTotal, 4);
  EXPECT_EQ(stringTotal, 10);

  EXPECT_EQ(lineage.size(), 2);

  EXPECT_EQ(lineage[0].parent_uid(), tinfo2->m_uid);
  EXPECT_EQ(lineage[0].parent_exec_file_path(), tinfo2->m_exepath);

  EXPECT_EQ(lineage[1].parent_uid(), tinfo->m_uid);
  EXPECT_EQ(lineage[1].parent_exec_file_path(), tinfo->m_exepath);

  CollectorStats::Reset();
}

TEST(ProcessSignalFormatterTest, ProcessWithTwoParentsWithTheSameNameTest) {
  sinsp* inspector = new_inspector();
  CollectorStats* collector_stats = CollectorStats::GetOrCreate();

  ProcessSignalFormatter processSignalFormatter(inspector);

  auto tinfo = std::make_shared<sinsp_threadinfo>(inspector);
  tinfo->m_pid = 3;
  tinfo->m_tid = 3;
  tinfo->m_vpid = 1;
  tinfo->m_uid = 42;
  tinfo->m_exepath = "asdf";

  auto tinfo2 = std::make_shared<sinsp_threadinfo>(inspector);
  tinfo2->m_pid = 1;
  tinfo2->m_tid = 1;
  tinfo2->m_ptid = 3;
  tinfo2->m_vpid = 2;
  tinfo2->m_uid = 7;
  tinfo2->m_exepath = "asdf";

  auto tinfo3 = std::make_shared<sinsp_threadinfo>(inspector);
  tinfo3->m_pid = 4;
  tinfo3->m_tid = 4;
  tinfo3->m_ptid = 1;
  tinfo3->m_vpid = 9;
  tinfo3->m_uid = 8;
  tinfo3->m_exepath = "asdf";

  inspector->add_thread(tinfo);
  inspector->add_thread(tinfo2);
  inspector->add_thread(tinfo3);

  std::vector<ProcessSignalFormatter::LineageInfo> lineage;
  processSignalFormatter.GetProcessLineage(tinfo3.get(), lineage);

  int count = collector_stats->GetCounter(CollectorStats::process_lineage_counts);
  int total = collector_stats->GetCounter(CollectorStats::process_lineage_total);
  int sqrTotal = collector_stats->GetCounter(CollectorStats::process_lineage_sqr_total);
  int stringTotal = collector_stats->GetCounter(CollectorStats::process_lineage_string_total);

  EXPECT_EQ(count, 1);
  EXPECT_EQ(total, 1);
  EXPECT_EQ(sqrTotal, 1);
  EXPECT_EQ(stringTotal, 4);

  EXPECT_EQ(lineage.size(), 1);

  EXPECT_EQ(lineage[0].parent_uid(), tinfo2->m_uid);
  EXPECT_EQ(lineage[0].parent_exec_file_path(), tinfo2->m_exepath);

  CollectorStats::Reset();
}

TEST(ProcessSignalFormatterTest, ProcessCollapseParentChildWithSameNameTest) {
  sinsp* inspector = new_inspector();
  CollectorStats* collector_stats = CollectorStats::GetOrCreate();

  ProcessSignalFormatter processSignalFormatter(inspector);

  auto tinfo = std::make_shared<sinsp_threadinfo>(inspector);
  tinfo->m_pid = 3;
  tinfo->m_tid = 3;
  tinfo->m_vpid = 1;
  tinfo->m_uid = 42;
  tinfo->m_exepath = "asdf";

  auto tinfo2 = std::make_shared<sinsp_threadinfo>(inspector);
  tinfo2->m_pid = 1;
  tinfo2->m_tid = 1;
  tinfo2->m_ptid = 3;
  tinfo2->m_vpid = 2;
  tinfo2->m_uid = 7;
  tinfo2->m_exepath = "asdf";

  auto tinfo3 = std::make_shared<sinsp_threadinfo>(inspector);
  tinfo3->m_pid = 4;
  tinfo3->m_tid = 4;
  tinfo3->m_ptid = 1;
  tinfo3->m_vpid = 9;
  tinfo3->m_uid = 8;
  tinfo3->m_exepath = "asdf";

  auto tinfo4 = std::make_shared<sinsp_threadinfo>(inspector);
  tinfo4->m_pid = 5;
  tinfo4->m_tid = 5;
  tinfo4->m_ptid = 4;
  tinfo4->m_vpid = 10;
  tinfo4->m_uid = 9;
  tinfo4->m_exepath = "qwerty";

  inspector->add_thread(tinfo);
  inspector->add_thread(tinfo2);
  inspector->add_thread(tinfo3);
  inspector->add_thread(tinfo4);

  std::vector<ProcessSignalFormatter::LineageInfo> lineage;
  processSignalFormatter.GetProcessLineage(tinfo4.get(), lineage);

  int count = collector_stats->GetCounter(CollectorStats::process_lineage_counts);
  int total = collector_stats->GetCounter(CollectorStats::process_lineage_total);
  int sqrTotal = collector_stats->GetCounter(CollectorStats::process_lineage_sqr_total);
  int stringTotal = collector_stats->GetCounter(CollectorStats::process_lineage_string_total);

  EXPECT_EQ(count, 1);
  EXPECT_EQ(total, 1);
  EXPECT_EQ(sqrTotal, 1);
  EXPECT_EQ(stringTotal, 4);

  EXPECT_EQ(lineage.size(), 1);

  EXPECT_EQ(lineage[0].parent_uid(), tinfo3->m_uid);
  EXPECT_EQ(lineage[0].parent_exec_file_path(), tinfo3->m_exepath);

  CollectorStats::Reset();
}

TEST(ProcessSignalFormatterTest, ProcessCollapseParentChildWithSameName2Test) {
  sinsp* inspector = new_inspector();
  CollectorStats* collector_stats = CollectorStats::GetOrCreate();

  ProcessSignalFormatter processSignalFormatter(inspector);

  auto tinfo = std::make_shared<sinsp_threadinfo>(inspector);
  tinfo->m_pid = 3;
  tinfo->m_tid = 3;
  tinfo->m_vpid = 1;
  tinfo->m_uid = 42;
  tinfo->m_exepath = "qwerty";

  auto tinfo2 = std::make_shared<sinsp_threadinfo>(inspector);
  tinfo2->m_pid = 1;
  tinfo2->m_tid = 1;
  tinfo2->m_ptid = 3;
  tinfo2->m_vpid = 2;
  tinfo2->m_uid = 7;
  tinfo2->m_exepath = "asdf";

  auto tinfo3 = std::make_shared<sinsp_threadinfo>(inspector);
  tinfo3->m_pid = 4;
  tinfo3->m_tid = 4;
  tinfo3->m_ptid = 1;
  tinfo3->m_vpid = 9;
  tinfo3->m_uid = 8;
  tinfo3->m_exepath = "asdf";

  auto tinfo4 = std::make_shared<sinsp_threadinfo>(inspector);
  tinfo4->m_pid = 5;
  tinfo4->m_tid = 5;
  tinfo4->m_ptid = 4;
  tinfo4->m_vpid = 10;
  tinfo4->m_uid = 9;
  tinfo4->m_exepath = "asdf";

  inspector->add_thread(tinfo);
  inspector->add_thread(tinfo2);
  inspector->add_thread(tinfo3);
  inspector->add_thread(tinfo4);

  std::vector<ProcessSignalFormatter::LineageInfo> lineage;
  processSignalFormatter.GetProcessLineage(tinfo4.get(), lineage);

  int count = collector_stats->GetCounter(CollectorStats::process_lineage_counts);
  int total = collector_stats->GetCounter(CollectorStats::process_lineage_total);
  int sqrTotal = collector_stats->GetCounter(CollectorStats::process_lineage_sqr_total);
  int stringTotal = collector_stats->GetCounter(CollectorStats::process_lineage_string_total);

  EXPECT_EQ(count, 1);
  EXPECT_EQ(total, 2);
  EXPECT_EQ(sqrTotal, 4);
  EXPECT_EQ(stringTotal, 10);

  EXPECT_EQ(lineage.size(), 2);

  EXPECT_EQ(lineage[0].parent_uid(), tinfo3->m_uid);
  EXPECT_EQ(lineage[0].parent_exec_file_path(), tinfo3->m_exepath);

  EXPECT_EQ(lineage[1].parent_uid(), tinfo->m_uid);
  EXPECT_EQ(lineage[1].parent_exec_file_path(), tinfo->m_exepath);

  CollectorStats::Reset();
}

TEST(ProcessSignalFormatterTest, ProcessWithUnrelatedProcessTest) {
  sinsp* inspector = new_inspector();
  CollectorStats* collector_stats = CollectorStats::GetOrCreate();

  ProcessSignalFormatter processSignalFormatter(inspector);

  auto tinfo = std::make_shared<sinsp_threadinfo>(inspector);
  tinfo->m_pid = 3;
  tinfo->m_tid = 3;
  tinfo->m_vpid = 1;
  tinfo->m_uid = 42;
  tinfo->m_exepath = "qwerty";

  auto tinfo2 = std::make_shared<sinsp_threadinfo>(inspector);
  tinfo2->m_pid = 1;
  tinfo2->m_tid = 1;
  tinfo2->m_ptid = 3;
  tinfo2->m_vpid = 2;
  tinfo2->m_uid = 7;
  tinfo2->m_exepath = "asdf";

  auto tinfo3 = std::make_shared<sinsp_threadinfo>(inspector);
  tinfo3->m_pid = 4;
  tinfo3->m_tid = 4;
  tinfo3->m_ptid = 1;
  tinfo3->m_vpid = 9;
  tinfo3->m_uid = 8;
  tinfo3->m_exepath = "uiop";

  auto tinfo4 = std::make_shared<sinsp_threadinfo>(inspector);
  tinfo4->m_pid = 5;
  tinfo4->m_tid = 5;
  tinfo4->m_ptid = 555;
  tinfo4->m_vpid = 10;
  tinfo4->m_uid = 9;
  tinfo4->m_exepath = "jkl;";

  inspector->add_thread(tinfo);
  inspector->add_thread(tinfo2);
  inspector->add_thread(tinfo3);
  inspector->add_thread(tinfo4);

  std::vector<ProcessSignalFormatter::LineageInfo> lineage;
  processSignalFormatter.GetProcessLineage(tinfo3.get(), lineage);

  int count = collector_stats->GetCounter(CollectorStats::process_lineage_counts);
  int total = collector_stats->GetCounter(CollectorStats::process_lineage_total);
  int sqrTotal = collector_stats->GetCounter(CollectorStats::process_lineage_sqr_total);
  int stringTotal = collector_stats->GetCounter(CollectorStats::process_lineage_string_total);

  EXPECT_EQ(count, 1);
  EXPECT_EQ(total, 2);
  EXPECT_EQ(sqrTotal, 4);
  EXPECT_EQ(stringTotal, 10);

  EXPECT_EQ(lineage.size(), 2);

  EXPECT_EQ(lineage[0].parent_uid(), tinfo2->m_uid);
  EXPECT_EQ(lineage[0].parent_exec_file_path(), tinfo2->m_exepath);

  EXPECT_EQ(lineage[1].parent_uid(), tinfo->m_uid);
  EXPECT_EQ(lineage[1].parent_exec_file_path(), tinfo->m_exepath);

  CollectorStats::Reset();
}

TEST(ProcessSignalFormatterTest, CountTwoCounterCallsTest) {
  sinsp* inspector = new_inspector();
  CollectorStats* collector_stats = CollectorStats::GetOrCreate();

  ProcessSignalFormatter processSignalFormatter(inspector);

  auto tinfo = std::make_shared<sinsp_threadinfo>(inspector);
  tinfo->m_pid = 1;
  tinfo->m_tid = 1;
  tinfo->m_ptid = 555;
  tinfo->m_vpid = 10;
  tinfo->m_uid = 9;
  tinfo->m_exepath = "jkl;";

  inspector->add_thread(tinfo);
  std::vector<LineageInfo> lineage;

  processSignalFormatter.GetProcessLineage(tinfo.get(), lineage);

  auto tinfo2 = std::make_shared<sinsp_threadinfo>(inspector);
  tinfo2->m_pid = 2;
  tinfo2->m_tid = 2;
  tinfo->m_ptid = 555;
  tinfo->m_vpid = 10;
  tinfo->m_uid = 9;
  tinfo->m_exepath = "jkl;";

  inspector->add_thread(tinfo2);
  std::vector<LineageInfo> lineage2;

  processSignalFormatter.GetProcessLineage(tinfo.get(), lineage2);

  int count = collector_stats->GetCounter(CollectorStats::process_lineage_counts);
  int total = collector_stats->GetCounter(CollectorStats::process_lineage_total);
  int sqrTotal = collector_stats->GetCounter(CollectorStats::process_lineage_sqr_total);
  int stringTotal = collector_stats->GetCounter(CollectorStats::process_lineage_string_total);

  EXPECT_EQ(count, 2);
  EXPECT_EQ(total, 0);
  EXPECT_EQ(sqrTotal, 0);
  EXPECT_EQ(stringTotal, 0);

  EXPECT_TRUE(lineage2.empty());

  CollectorStats::Reset();
}

}  // namespace

}  // namespace collector
