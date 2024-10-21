// clang-format off
#include <Utility.h>
#include "libsinsp/sinsp.h"
// clang-format on

#include "CollectorStats.h"
#include "ProcessSignalFormatter.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace collector {

using LineageInfo = ProcessSignalFormatter::LineageInfo;
using namespace testing;

class MockCollectorConfig : public CollectorConfig {
 public:
  MockCollectorConfig() = default;

  void SetDisableProcessArguments(bool value) {
    disable_process_arguments_ = value;
  }
};

TEST(ProcessSignalFormatterTest, NoProcessTest) {
  sinsp* inspector = NULL;
  CollectorStats& collector_stats = CollectorStats::GetOrCreate();
  CollectorConfig config;

  ProcessSignalFormatter processSignalFormatter(inspector, config);

  sinsp_threadinfo* tinfo = NULL;
  std::vector<LineageInfo> lineage;

  processSignalFormatter.GetProcessLineage(tinfo, lineage);

  int count = collector_stats.GetCounter(CollectorStats::process_lineage_counts);
  int total = collector_stats.GetCounter(CollectorStats::process_lineage_total);
  int sqrTotal = collector_stats.GetCounter(CollectorStats::process_lineage_sqr_total);
  int stringTotal = collector_stats.GetCounter(CollectorStats::process_lineage_string_total);

  EXPECT_EQ(count, 0);
  EXPECT_EQ(total, 0);
  EXPECT_EQ(sqrTotal, 0);
  EXPECT_EQ(stringTotal, 0);

  EXPECT_TRUE(lineage.empty());
}

TEST(ProcessSignalFormatterTest, ProcessWithoutParentTest) {
  std::unique_ptr<sinsp> inspector(new sinsp());
  CollectorStats& collector_stats = CollectorStats::GetOrCreate();
  CollectorConfig config;

  ProcessSignalFormatter processSignalFormatter(inspector.get(), config);

  auto tinfo = inspector->build_threadinfo();
  tinfo->m_pid = 0;
  tinfo->m_tid = 0;
  tinfo->m_ptid = -1;
  tinfo->m_vpid = 2;
  tinfo->m_user.set_uid(7);
  tinfo->m_exepath = "qwerty";

  inspector->add_thread(std::move(tinfo));
  std::vector<LineageInfo> lineage;

  processSignalFormatter.GetProcessLineage(inspector->get_thread_ref(0).get(), lineage);

  int count = collector_stats.GetCounter(CollectorStats::process_lineage_counts);
  int total = collector_stats.GetCounter(CollectorStats::process_lineage_total);
  int sqrTotal = collector_stats.GetCounter(CollectorStats::process_lineage_sqr_total);
  int stringTotal = collector_stats.GetCounter(CollectorStats::process_lineage_string_total);

  EXPECT_EQ(count, 1);
  EXPECT_EQ(total, 0);
  EXPECT_EQ(sqrTotal, 0);
  EXPECT_EQ(stringTotal, 0);

  EXPECT_TRUE(lineage.empty());

  CollectorStats::Reset();
}

TEST(ProcessSignalFormatterTest, ProcessWithParentTest) {
  std::unique_ptr<sinsp> inspector(new sinsp());
  CollectorStats& collector_stats = CollectorStats::GetOrCreate();
  CollectorConfig config;

  ProcessSignalFormatter processSignalFormatter(inspector.get(), config);

  auto tinfo = inspector->build_threadinfo();
  tinfo->m_pid = 3;
  tinfo->m_tid = 3;
  tinfo->m_ptid = -1;
  tinfo->m_vpid = 1;
  tinfo->m_user.set_uid(42);
  tinfo->m_exepath = "asdf";
  auto tinfo2 = inspector->build_threadinfo();
  tinfo2->m_pid = 1;
  tinfo2->m_tid = 1;
  tinfo2->m_ptid = 3;
  tinfo2->m_vpid = 2;
  tinfo2->m_user.set_uid(7);
  tinfo2->m_exepath = "qwerty";
  inspector->add_thread(std::move(tinfo));
  inspector->add_thread(std::move(tinfo2));

  std::vector<ProcessSignalFormatter::LineageInfo> lineage;
  processSignalFormatter.GetProcessLineage(inspector->get_thread_ref(1).get(), lineage);

  int count = collector_stats.GetCounter(CollectorStats::process_lineage_counts);
  int total = collector_stats.GetCounter(CollectorStats::process_lineage_total);
  int sqrTotal = collector_stats.GetCounter(CollectorStats::process_lineage_sqr_total);
  int stringTotal = collector_stats.GetCounter(CollectorStats::process_lineage_string_total);

  EXPECT_EQ(count, 1);
  EXPECT_EQ(total, 1);
  EXPECT_EQ(sqrTotal, 1);
  EXPECT_EQ(stringTotal, 4);

  EXPECT_EQ(lineage.size(), 1);

  EXPECT_EQ(lineage[0].parent_uid(), 42);
  EXPECT_EQ(lineage[0].parent_exec_file_path(), "asdf");

  CollectorStats::Reset();
}

TEST(ProcessSignalFormatterTest, ProcessWithParentWithPid0Test) {
  std::unique_ptr<sinsp> inspector(new sinsp());
  CollectorStats& collector_stats = CollectorStats::GetOrCreate();
  CollectorConfig config;

  ProcessSignalFormatter processSignalFormatter(inspector.get(), config);

  auto tinfo = inspector->build_threadinfo();
  tinfo->m_pid = 0;
  tinfo->m_tid = 0;
  tinfo->m_ptid = -1;
  tinfo->m_vpid = 1;
  tinfo->m_exepath = "asdf";
  auto tinfo2 = inspector->build_threadinfo();
  tinfo2->m_pid = 1;
  tinfo2->m_tid = 1;
  tinfo2->m_ptid = 0;
  tinfo2->m_vpid = 2;
  tinfo2->m_exepath = "qwerty";
  inspector->add_thread(std::move(tinfo));
  inspector->add_thread(std::move(tinfo2));

  std::vector<ProcessSignalFormatter::LineageInfo> lineage;
  processSignalFormatter.GetProcessLineage(inspector->get_thread_ref(1).get(), lineage);

  int count = collector_stats.GetCounter(CollectorStats::process_lineage_counts);
  int total = collector_stats.GetCounter(CollectorStats::process_lineage_total);
  int sqrTotal = collector_stats.GetCounter(CollectorStats::process_lineage_sqr_total);
  int stringTotal = collector_stats.GetCounter(CollectorStats::process_lineage_string_total);

  EXPECT_EQ(count, 1);
  EXPECT_EQ(total, 0);
  EXPECT_EQ(sqrTotal, 0);
  EXPECT_EQ(stringTotal, 0);

  EXPECT_TRUE(lineage.empty());

  CollectorStats::Reset();
}

TEST(ProcessSignalFormatterTest, ProcessWithParentWithSameNameTest) {
  std::unique_ptr<sinsp> inspector(new sinsp());
  CollectorStats& collector_stats = CollectorStats::GetOrCreate();
  CollectorConfig config;

  ProcessSignalFormatter processSignalFormatter(inspector.get(), config);

  auto tinfo = inspector->build_threadinfo();
  tinfo->m_pid = 3;
  tinfo->m_tid = 3;
  tinfo->m_ptid = -1;
  tinfo->m_vpid = 1;
  tinfo->m_user.set_uid(43);
  tinfo->m_exepath = "asdf";
  auto tinfo2 = inspector->build_threadinfo();
  tinfo2->m_pid = 1;
  tinfo2->m_tid = 1;
  tinfo2->m_ptid = 3;
  tinfo2->m_vpid = 2;
  tinfo2->m_user.set_uid(42);
  tinfo2->m_exepath = "asdf";
  inspector->add_thread(std::move(tinfo));
  inspector->add_thread(std::move(tinfo2));

  std::vector<ProcessSignalFormatter::LineageInfo> lineage;
  processSignalFormatter.GetProcessLineage(inspector->get_thread_ref(1).get(), lineage);

  int count = collector_stats.GetCounter(CollectorStats::process_lineage_counts);
  int total = collector_stats.GetCounter(CollectorStats::process_lineage_total);
  int sqrTotal = collector_stats.GetCounter(CollectorStats::process_lineage_sqr_total);
  int stringTotal = collector_stats.GetCounter(CollectorStats::process_lineage_string_total);

  EXPECT_EQ(count, 1);
  EXPECT_EQ(total, 1);
  EXPECT_EQ(sqrTotal, 1);
  EXPECT_EQ(stringTotal, 4);

  EXPECT_EQ(lineage.size(), 1);

  EXPECT_EQ(lineage[0].parent_uid(), 43);
  EXPECT_EQ(lineage[0].parent_exec_file_path(), "asdf");

  CollectorStats::Reset();
}

TEST(ProcessSignalFormatterTest, ProcessWithTwoParentsTest) {
  std::unique_ptr<sinsp> inspector(new sinsp());
  CollectorStats& collector_stats = CollectorStats::GetOrCreate();
  CollectorConfig config;

  ProcessSignalFormatter processSignalFormatter(inspector.get(), config);

  auto tinfo = inspector->build_threadinfo();
  tinfo->m_pid = 3;
  tinfo->m_tid = 3;
  tinfo->m_ptid = -1;
  tinfo->m_vpid = 1;
  tinfo->m_user.set_uid(42);
  tinfo->m_exepath = "asdf";

  auto tinfo2 = inspector->build_threadinfo();
  tinfo2->m_pid = 1;
  tinfo2->m_tid = 1;
  tinfo2->m_ptid = 3;
  tinfo2->m_vpid = 2;
  tinfo2->m_user.set_uid(7);
  tinfo2->m_exepath = "qwerty";

  auto tinfo3 = inspector->build_threadinfo();
  tinfo3->m_pid = 4;
  tinfo3->m_tid = 4;
  tinfo3->m_ptid = 1;
  tinfo3->m_vpid = 9;
  tinfo3->m_user.set_uid(8);
  tinfo3->m_exepath = "uiop";

  inspector->add_thread(std::move(tinfo));
  inspector->add_thread(std::move(tinfo2));
  inspector->add_thread(std::move(tinfo3));

  std::vector<ProcessSignalFormatter::LineageInfo> lineage;
  processSignalFormatter.GetProcessLineage(inspector->get_thread_ref(4).get(), lineage);

  int count = collector_stats.GetCounter(CollectorStats::process_lineage_counts);
  int total = collector_stats.GetCounter(CollectorStats::process_lineage_total);
  int sqrTotal = collector_stats.GetCounter(CollectorStats::process_lineage_sqr_total);
  int stringTotal = collector_stats.GetCounter(CollectorStats::process_lineage_string_total);

  EXPECT_EQ(count, 1);
  EXPECT_EQ(total, 2);
  EXPECT_EQ(sqrTotal, 4);
  EXPECT_EQ(stringTotal, 10);

  EXPECT_EQ(lineage.size(), 2);

  EXPECT_EQ(lineage[0].parent_uid(), 7);
  EXPECT_EQ(lineage[0].parent_exec_file_path(), "qwerty");

  EXPECT_EQ(lineage[1].parent_uid(), 42);
  EXPECT_EQ(lineage[1].parent_exec_file_path(), "asdf");

  CollectorStats::Reset();
}

TEST(ProcessSignalFormatterTest, ProcessWithTwoParentsWithTheSameNameTest) {
  std::unique_ptr<sinsp> inspector(new sinsp());
  CollectorStats& collector_stats = CollectorStats::GetOrCreate();
  CollectorConfig config;

  ProcessSignalFormatter processSignalFormatter(inspector.get(), config);

  auto tinfo = inspector->build_threadinfo();
  tinfo->m_pid = 3;
  tinfo->m_tid = 3;
  tinfo->m_ptid = -1;
  tinfo->m_vpid = 1;
  tinfo->m_user.set_uid(42);
  tinfo->m_exepath = "asdf";

  auto tinfo2 = inspector->build_threadinfo();
  tinfo2->m_pid = 1;
  tinfo2->m_tid = 1;
  tinfo2->m_ptid = 3;
  tinfo2->m_vpid = 2;
  tinfo2->m_user.set_uid(7);
  tinfo2->m_exepath = "asdf";

  auto tinfo3 = inspector->build_threadinfo();
  tinfo3->m_pid = 4;
  tinfo3->m_tid = 4;
  tinfo3->m_ptid = 1;
  tinfo3->m_vpid = 9;
  tinfo3->m_user.set_uid(8);
  tinfo3->m_exepath = "asdf";

  inspector->add_thread(std::move(tinfo));
  inspector->add_thread(std::move(tinfo2));
  inspector->add_thread(std::move(tinfo3));

  std::vector<ProcessSignalFormatter::LineageInfo> lineage;
  processSignalFormatter.GetProcessLineage(inspector->get_thread_ref(4).get(), lineage);

  int count = collector_stats.GetCounter(CollectorStats::process_lineage_counts);
  int total = collector_stats.GetCounter(CollectorStats::process_lineage_total);
  int sqrTotal = collector_stats.GetCounter(CollectorStats::process_lineage_sqr_total);
  int stringTotal = collector_stats.GetCounter(CollectorStats::process_lineage_string_total);

  EXPECT_EQ(count, 1);
  EXPECT_EQ(total, 1);
  EXPECT_EQ(sqrTotal, 1);
  EXPECT_EQ(stringTotal, 4);

  EXPECT_EQ(lineage.size(), 1);

  EXPECT_EQ(lineage[0].parent_uid(), 7);
  EXPECT_EQ(lineage[0].parent_exec_file_path(), "asdf");

  CollectorStats::Reset();
}

TEST(ProcessSignalFormatterTest, ProcessCollapseParentChildWithSameNameTest) {
  std::unique_ptr<sinsp> inspector(new sinsp());
  CollectorStats& collector_stats = CollectorStats::GetOrCreate();
  CollectorConfig config;

  ProcessSignalFormatter processSignalFormatter(inspector.get(), config);

  auto tinfo = inspector->build_threadinfo();
  tinfo->m_pid = 3;
  tinfo->m_tid = 3;
  tinfo->m_ptid = -1;
  tinfo->m_vpid = 1;
  tinfo->m_user.set_uid(42);
  tinfo->m_exepath = "asdf";

  auto tinfo2 = inspector->build_threadinfo();
  tinfo2->m_pid = 1;
  tinfo2->m_tid = 1;
  tinfo2->m_ptid = 3;
  tinfo2->m_vpid = 2;
  tinfo2->m_user.set_uid(7);
  tinfo2->m_exepath = "asdf";

  auto tinfo3 = inspector->build_threadinfo();
  tinfo3->m_pid = 4;
  tinfo3->m_tid = 4;
  tinfo3->m_ptid = 1;
  tinfo3->m_vpid = 9;
  tinfo3->m_user.set_uid(8);
  tinfo3->m_exepath = "asdf";

  auto tinfo4 = inspector->build_threadinfo();
  tinfo4->m_pid = 5;
  tinfo4->m_tid = 5;
  tinfo4->m_ptid = 4;
  tinfo4->m_vpid = 10;
  tinfo4->m_user.set_uid(9);
  tinfo4->m_exepath = "qwerty";

  inspector->add_thread(std::move(tinfo));
  inspector->add_thread(std::move(tinfo2));
  inspector->add_thread(std::move(tinfo3));
  inspector->add_thread(std::move(tinfo4));

  std::vector<ProcessSignalFormatter::LineageInfo> lineage;
  processSignalFormatter.GetProcessLineage(inspector->get_thread_ref(5).get(), lineage);

  int count = collector_stats.GetCounter(CollectorStats::process_lineage_counts);
  int total = collector_stats.GetCounter(CollectorStats::process_lineage_total);
  int sqrTotal = collector_stats.GetCounter(CollectorStats::process_lineage_sqr_total);
  int stringTotal = collector_stats.GetCounter(CollectorStats::process_lineage_string_total);

  EXPECT_EQ(count, 1);
  EXPECT_EQ(total, 1);
  EXPECT_EQ(sqrTotal, 1);
  EXPECT_EQ(stringTotal, 4);

  EXPECT_EQ(lineage.size(), 1);

  EXPECT_EQ(lineage[0].parent_uid(), 8);
  EXPECT_EQ(lineage[0].parent_exec_file_path(), "asdf");

  CollectorStats::Reset();
}

TEST(ProcessSignalFormatterTest, ProcessCollapseParentChildWithSameName2Test) {
  std::unique_ptr<sinsp> inspector(new sinsp());
  CollectorStats& collector_stats = CollectorStats::GetOrCreate();
  CollectorConfig config;

  ProcessSignalFormatter processSignalFormatter(inspector.get(), config);

  auto tinfo = inspector->build_threadinfo();
  tinfo->m_pid = 3;
  tinfo->m_tid = 3;
  tinfo->m_ptid = -1;
  tinfo->m_vpid = 1;
  tinfo->m_user.set_uid(42);
  tinfo->m_exepath = "qwerty";

  auto tinfo2 = inspector->build_threadinfo();
  tinfo2->m_pid = 1;
  tinfo2->m_tid = 1;
  tinfo2->m_ptid = 3;
  tinfo2->m_vpid = 2;
  tinfo2->m_user.set_uid(7);
  tinfo2->m_exepath = "asdf";

  auto tinfo3 = inspector->build_threadinfo();
  tinfo3->m_pid = 4;
  tinfo3->m_tid = 4;
  tinfo3->m_ptid = 1;
  tinfo3->m_vpid = 9;
  tinfo3->m_user.set_uid(8);
  tinfo3->m_exepath = "asdf";

  auto tinfo4 = inspector->build_threadinfo();
  tinfo4->m_pid = 5;
  tinfo4->m_tid = 5;
  tinfo4->m_ptid = 4;
  tinfo4->m_vpid = 10;
  tinfo4->m_user.set_uid(9);
  tinfo4->m_exepath = "asdf";

  inspector->add_thread(std::move(tinfo));
  inspector->add_thread(std::move(tinfo2));
  inspector->add_thread(std::move(tinfo3));
  inspector->add_thread(std::move(tinfo4));

  std::vector<ProcessSignalFormatter::LineageInfo> lineage;
  processSignalFormatter.GetProcessLineage(inspector->get_thread_ref(5).get(), lineage);

  int count = collector_stats.GetCounter(CollectorStats::process_lineage_counts);
  int total = collector_stats.GetCounter(CollectorStats::process_lineage_total);
  int sqrTotal = collector_stats.GetCounter(CollectorStats::process_lineage_sqr_total);
  int stringTotal = collector_stats.GetCounter(CollectorStats::process_lineage_string_total);

  EXPECT_EQ(count, 1);
  EXPECT_EQ(total, 2);
  EXPECT_EQ(sqrTotal, 4);
  EXPECT_EQ(stringTotal, 10);

  EXPECT_EQ(lineage.size(), 2);

  EXPECT_EQ(lineage[0].parent_uid(), 8);
  EXPECT_EQ(lineage[0].parent_exec_file_path(), "asdf");

  EXPECT_EQ(lineage[1].parent_uid(), 42);
  EXPECT_EQ(lineage[1].parent_exec_file_path(), "qwerty");

  CollectorStats::Reset();
}

TEST(ProcessSignalFormatterTest, ProcessWithUnrelatedProcessTest) {
  std::unique_ptr<sinsp> inspector(new sinsp());
  CollectorStats& collector_stats = CollectorStats::GetOrCreate();
  CollectorConfig config;

  ProcessSignalFormatter processSignalFormatter(inspector.get(), config);

  auto tinfo = inspector->build_threadinfo();
  tinfo->m_pid = 3;
  tinfo->m_tid = 3;
  tinfo->m_ptid = -1;
  tinfo->m_vpid = 1;
  tinfo->m_user.set_uid(42);
  tinfo->m_exepath = "qwerty";

  auto tinfo2 = inspector->build_threadinfo();
  tinfo2->m_pid = 1;
  tinfo2->m_tid = 1;
  tinfo2->m_ptid = 3;
  tinfo2->m_vpid = 2;
  tinfo2->m_user.set_uid(7);
  tinfo2->m_exepath = "asdf";

  auto tinfo3 = inspector->build_threadinfo();
  tinfo3->m_pid = 4;
  tinfo3->m_tid = 4;
  tinfo3->m_ptid = 1;
  tinfo3->m_vpid = 9;
  tinfo3->m_user.set_uid(8);
  tinfo3->m_exepath = "uiop";

  auto tinfo4 = inspector->build_threadinfo();
  tinfo4->m_pid = 5;
  tinfo4->m_tid = 5;
  tinfo4->m_ptid = 555;
  tinfo4->m_vpid = 10;
  tinfo4->m_user.set_uid(9);
  tinfo4->m_exepath = "jkl;";

  inspector->add_thread(std::move(tinfo));
  inspector->add_thread(std::move(tinfo2));
  inspector->add_thread(std::move(tinfo3));
  inspector->add_thread(std::move(tinfo4));

  std::vector<ProcessSignalFormatter::LineageInfo> lineage;
  processSignalFormatter.GetProcessLineage(inspector->get_thread_ref(4).get(), lineage);

  int count = collector_stats.GetCounter(CollectorStats::process_lineage_counts);
  int total = collector_stats.GetCounter(CollectorStats::process_lineage_total);
  int sqrTotal = collector_stats.GetCounter(CollectorStats::process_lineage_sqr_total);
  int stringTotal = collector_stats.GetCounter(CollectorStats::process_lineage_string_total);

  EXPECT_EQ(count, 1);
  EXPECT_EQ(total, 2);
  EXPECT_EQ(sqrTotal, 4);
  EXPECT_EQ(stringTotal, 10);

  EXPECT_EQ(lineage.size(), 2);

  EXPECT_EQ(lineage[0].parent_uid(), 7);
  EXPECT_EQ(lineage[0].parent_exec_file_path(), "asdf");

  EXPECT_EQ(lineage[1].parent_uid(), 42);
  EXPECT_EQ(lineage[1].parent_exec_file_path(), "qwerty");

  CollectorStats::Reset();
}

TEST(ProcessSignalFormatterTest, CountTwoCounterCallsTest) {
  std::unique_ptr<sinsp> inspector(new sinsp());
  CollectorStats& collector_stats = CollectorStats::GetOrCreate();
  CollectorConfig config;

  ProcessSignalFormatter processSignalFormatter(inspector.get(), config);

  auto tinfo = inspector->build_threadinfo();
  tinfo->m_pid = 1;
  tinfo->m_tid = 1;
  tinfo->m_ptid = 555;
  tinfo->m_vpid = 10;
  tinfo->m_user.set_uid(9);
  tinfo->m_exepath = "jkl;";

  inspector->add_thread(std::move(tinfo));
  std::vector<LineageInfo> lineage;

  processSignalFormatter.GetProcessLineage(inspector->get_thread_ref(1).get(), lineage);

  auto tinfo2 = inspector->build_threadinfo();
  tinfo2->m_pid = 2;
  tinfo2->m_tid = 2;
  tinfo2->m_ptid = 555;
  tinfo2->m_vpid = 10;
  tinfo2->m_user.set_uid(9);
  tinfo2->m_exepath = "jkl;";

  inspector->add_thread(std::move(tinfo2));
  std::vector<LineageInfo> lineage2;

  processSignalFormatter.GetProcessLineage(inspector->get_thread_ref(2).get(), lineage2);

  int count = collector_stats.GetCounter(CollectorStats::process_lineage_counts);
  int total = collector_stats.GetCounter(CollectorStats::process_lineage_total);
  int sqrTotal = collector_stats.GetCounter(CollectorStats::process_lineage_sqr_total);
  int stringTotal = collector_stats.GetCounter(CollectorStats::process_lineage_string_total);

  EXPECT_EQ(count, 2);
  EXPECT_EQ(total, 0);
  EXPECT_EQ(sqrTotal, 0);
  EXPECT_EQ(stringTotal, 0);

  EXPECT_TRUE(lineage2.empty());

  CollectorStats::Reset();
}

TEST(ProcessSignalFormatterTest, Rox3377ProcessLineageWithNoVPidTest) {
  std::unique_ptr<sinsp> inspector(new sinsp());
  CollectorStats& collector_stats = CollectorStats::GetOrCreate();
  CollectorConfig config;

  ProcessSignalFormatter processSignalFormatter(inspector.get(), config);

  auto tinfo = inspector->build_threadinfo();
  tinfo->m_pid = 3;
  tinfo->m_tid = 3;
  tinfo->m_ptid = -1;
  tinfo->m_vpid = 0;
  tinfo->m_user.set_uid(42);
  tinfo->m_container_id = "";
  tinfo->m_exepath = "qwerty";

  auto tinfo2 = inspector->build_threadinfo();
  tinfo2->m_pid = 1;
  tinfo2->m_tid = 1;
  tinfo2->m_ptid = 3;
  tinfo2->m_vpid = 0;
  tinfo2->m_user.set_uid(7);
  tinfo2->m_container_id = "id";
  tinfo2->m_exepath = "asdf";

  auto tinfo3 = inspector->build_threadinfo();
  tinfo3->m_pid = 4;
  tinfo3->m_tid = 4;
  tinfo3->m_ptid = 1;
  tinfo3->m_vpid = 0;
  tinfo3->m_user.set_uid(8);
  tinfo3->m_container_id = "id";
  tinfo3->m_exepath = "uiop";

  inspector->add_thread(std::move(tinfo));
  inspector->add_thread(std::move(tinfo2));
  inspector->add_thread(std::move(tinfo3));

  std::vector<ProcessSignalFormatter::LineageInfo> lineage;
  processSignalFormatter.GetProcessLineage(inspector->get_thread_ref(4).get(), lineage);

  int count = collector_stats.GetCounter(CollectorStats::process_lineage_counts);
  int total = collector_stats.GetCounter(CollectorStats::process_lineage_total);
  int sqrTotal = collector_stats.GetCounter(CollectorStats::process_lineage_sqr_total);
  int stringTotal = collector_stats.GetCounter(CollectorStats::process_lineage_string_total);

  EXPECT_EQ(count, 1);
  EXPECT_EQ(total, 1);
  EXPECT_EQ(sqrTotal, 1);
  EXPECT_EQ(stringTotal, 4);

  EXPECT_EQ(lineage.size(), 1);

  EXPECT_EQ(lineage[0].parent_uid(), 7);
  EXPECT_EQ(lineage[0].parent_exec_file_path(), "asdf");

  CollectorStats::Reset();
}

TEST(ProcessSignalFormatterTest, ProcessArguments) {
  std::unique_ptr<sinsp> inspector(new sinsp());
  MockCollectorConfig config;

  ProcessSignalFormatter processSignalFormatter(inspector.get(), config);

  auto tinfo = inspector->build_threadinfo();
  tinfo->m_pid = 3;
  tinfo->m_tid = 3;
  tinfo->m_ptid = -1;
  tinfo->m_vpid = 0;
  tinfo->m_user.set_uid(42);
  tinfo->m_container_id = "";
  tinfo->m_exepath = "qwerty";

  std::vector<std::string> args = {std::string("args")};
  tinfo->set_args(args);

  std::unique_ptr<sinsp_evt> evt(new sinsp_evt());
  std::unique_ptr<scap_evt> s_evt(new scap_evt());

  s_evt->type = PPME_SYSCALL_EXECVE_19_X;
  evt.get()->set_tinfo(tinfo.get());
  evt.get()->set_scap_evt(s_evt.get());

  auto signal = processSignalFormatter.CreateProcessSignal(evt.get());
  EXPECT_FALSE(signal->args().empty());
}

TEST(ProcessSignalFormatterTest, NoProcessArguments) {
  std::unique_ptr<sinsp> inspector(new sinsp());
  MockCollectorConfig config;

  config.SetDisableProcessArguments(true);
  ProcessSignalFormatter processSignalFormatter(inspector.get(), config);

  auto tinfo = inspector->build_threadinfo();
  tinfo->m_pid = 3;
  tinfo->m_tid = 3;
  tinfo->m_ptid = -1;
  tinfo->m_vpid = 0;
  tinfo->m_user.set_uid(42);
  tinfo->m_container_id = "";
  tinfo->m_exepath = "qwerty";

  std::vector<std::string> args = {std::string("args")};
  tinfo->set_args(args);

  std::unique_ptr<sinsp_evt> evt(new sinsp_evt());
  std::unique_ptr<scap_evt> s_evt(new scap_evt());

  s_evt->type = PPME_SYSCALL_EXECVE_19_X;
  evt.get()->set_tinfo(tinfo.get());
  evt.get()->set_scap_evt(s_evt.get());

  auto signal = processSignalFormatter.CreateProcessSignal(evt.get());
  EXPECT_TRUE(signal->args().empty());
}

}  // namespace collector
