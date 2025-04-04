#include <cstdint>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "libsinsp/sinsp.h"

#include "CollectorStats.h"
#include "SensorClientFormatter.h"
#include "Utility.h"

namespace collector {

using LineageInfo = SensorClientFormatter::LineageInfo;

struct ThreadInfoParams {
  int64_t pid;
  int64_t tid;
  int64_t ptid;
  int64_t vpid;
  int64_t uid;
  std::string container_id;
  std::string exepath;
};

#define EXPECT_STATS_COUNTER(index, expected) \
  EXPECT_EQ(CollectorStats::GetOrCreate().GetCounter(index), expected)

class SensorClientFormatterTest : public testing::Test {
 public:
  SensorClientFormatterTest() : inspector(new sinsp()), formatter(inspector.get(), config) {
  }

 protected:
  std::unique_ptr<sinsp_threadinfo> build_threadinfo(const ThreadInfoParams& params) {
    auto tinfo = inspector->build_threadinfo();
    tinfo->m_pid = params.pid;
    tinfo->m_tid = params.tid;
    tinfo->m_ptid = params.ptid;
    tinfo->m_vpid = params.vpid;
    tinfo->m_user.set_uid(params.uid);
    tinfo->m_container_id = params.container_id;
    tinfo->m_exepath = params.exepath;
    return tinfo;
  }

  std::unique_ptr<sinsp> inspector;
  CollectorConfig config;
  SensorClientFormatter formatter;
};

TEST_F(SensorClientFormatterTest, NoProcessTest) {
  sinsp_threadinfo* tinfo = nullptr;
  auto lineage = SensorClientFormatter::GetProcessLineage(tinfo);

  EXPECT_STATS_COUNTER(CollectorStats::process_lineage_counts, 0);
  EXPECT_STATS_COUNTER(CollectorStats::process_lineage_total, 0);
  EXPECT_STATS_COUNTER(CollectorStats::process_lineage_sqr_total, 0);
  EXPECT_STATS_COUNTER(CollectorStats::process_lineage_string_total, 0);

  EXPECT_TRUE(lineage.empty());
}

TEST_F(SensorClientFormatterTest, ProcessWithoutParentTest) {
  // {pid, tid, ptid, vpid, uid, container_id, exepath},
  inspector->add_thread(build_threadinfo({0, 0, -1, 2, 7, "", "qwerty"}));
  auto lineage = SensorClientFormatter::GetProcessLineage(inspector->get_thread_ref(0).get());

  EXPECT_STATS_COUNTER(CollectorStats::process_lineage_counts, 1);
  EXPECT_STATS_COUNTER(CollectorStats::process_lineage_total, 0);
  EXPECT_STATS_COUNTER(CollectorStats::process_lineage_sqr_total, 0);
  EXPECT_STATS_COUNTER(CollectorStats::process_lineage_string_total, 0);

  EXPECT_TRUE(lineage.empty());

  CollectorStats::Reset();
}

TEST_F(SensorClientFormatterTest, ProcessWithParentTest) {
  // {pid, tid, ptid, vpid, uid, container_id, exepath},
  std::vector<ThreadInfoParams> tinfo_params = {
      {3, 3, -1, 1, 42, "", "asdf"},
      {1, 1, 3, 2, 7, "", "qwerty"},
  };

  for (const auto& params : tinfo_params) {
    inspector->add_thread(build_threadinfo(params));
  }

  auto lineage = SensorClientFormatter::GetProcessLineage(inspector->get_thread_ref(1).get());

  EXPECT_STATS_COUNTER(CollectorStats::process_lineage_counts, 1);
  EXPECT_STATS_COUNTER(CollectorStats::process_lineage_total, 1);
  EXPECT_STATS_COUNTER(CollectorStats::process_lineage_sqr_total, 1);
  EXPECT_STATS_COUNTER(CollectorStats::process_lineage_string_total, 4);

  EXPECT_EQ(lineage.size(), 1);

  EXPECT_EQ(lineage[0].parent_uid(), 42);
  EXPECT_EQ(lineage[0].parent_exec_file_path(), "asdf");

  CollectorStats::Reset();
}

TEST_F(SensorClientFormatterTest, ProcessWithParentWithPid0Test) {
  // {pid, tid, ptid, vpid, uid, container_id, exepath},
  std::vector<ThreadInfoParams> tinfo_params = {
      {0, 0, -1, 1, 0, "", "asdf"},
      {1, 1, 0, 2, 0, "", "qwerty"},
  };

  for (const auto& params : tinfo_params) {
    inspector->add_thread(build_threadinfo(params));
  }

  auto lineage = SensorClientFormatter::GetProcessLineage(inspector->get_thread_ref(1).get());

  EXPECT_STATS_COUNTER(CollectorStats::process_lineage_counts, 1);
  EXPECT_STATS_COUNTER(CollectorStats::process_lineage_total, 0);
  EXPECT_STATS_COUNTER(CollectorStats::process_lineage_sqr_total, 0);
  EXPECT_STATS_COUNTER(CollectorStats::process_lineage_string_total, 0);

  EXPECT_TRUE(lineage.empty());

  CollectorStats::Reset();
}

TEST_F(SensorClientFormatterTest, ProcessWithParentWithSameNameTest) {
  // {pid, tid, ptid, vpid, uid, container_id, exepath},
  std::vector<ThreadInfoParams> tinfo_params = {
      {3, 3, -1, 1, 43, "", "asdf"},
      {1, 1, 3, 2, 42, "", "asdf"},
  };

  for (const auto& params : tinfo_params) {
    inspector->add_thread(build_threadinfo(params));
  }

  auto lineage = SensorClientFormatter::GetProcessLineage(inspector->get_thread_ref(1).get());

  EXPECT_STATS_COUNTER(CollectorStats::process_lineage_counts, 1);
  EXPECT_STATS_COUNTER(CollectorStats::process_lineage_total, 1);
  EXPECT_STATS_COUNTER(CollectorStats::process_lineage_sqr_total, 1);
  EXPECT_STATS_COUNTER(CollectorStats::process_lineage_string_total, 4);

  EXPECT_EQ(lineage.size(), 1);

  EXPECT_EQ(lineage[0].parent_uid(), 43);
  EXPECT_EQ(lineage[0].parent_exec_file_path(), "asdf");

  CollectorStats::Reset();
}

TEST_F(SensorClientFormatterTest, ProcessWithTwoParentsTest) {
  // {pid, tid, ptid, vpid, uid, container_id, exepath},
  std::vector<ThreadInfoParams> tinfo_params = {
      {3, 3, -1, 1, 42, "", "asdf"},
      {1, 1, 3, 2, 7, "", "qwerty"},
      {4, 4, 1, 9, 8, "", "uiop"},
  };

  for (const auto& params : tinfo_params) {
    inspector->add_thread(build_threadinfo(params));
  }

  auto lineage = SensorClientFormatter::GetProcessLineage(inspector->get_thread_ref(4).get());

  EXPECT_STATS_COUNTER(CollectorStats::process_lineage_counts, 1);
  EXPECT_STATS_COUNTER(CollectorStats::process_lineage_total, 2);
  EXPECT_STATS_COUNTER(CollectorStats::process_lineage_sqr_total, 4);
  EXPECT_STATS_COUNTER(CollectorStats::process_lineage_string_total, 10);

  EXPECT_EQ(lineage.size(), 2);

  EXPECT_EQ(lineage[0].parent_uid(), 7);
  EXPECT_EQ(lineage[0].parent_exec_file_path(), "qwerty");

  EXPECT_EQ(lineage[1].parent_uid(), 42);
  EXPECT_EQ(lineage[1].parent_exec_file_path(), "asdf");

  CollectorStats::Reset();
}

TEST_F(SensorClientFormatterTest, ProcessWithTwoParentsWithTheSameNameTest) {
  // {pid, tid, ptid, vpid, uid, container_id, exepath},
  std::vector<ThreadInfoParams> tinfo_params = {
      {3, 3, -1, 1, 42, "", "asdf"},
      {1, 1, 3, 2, 7, "", "asdf"},
      {4, 4, 1, 9, 8, "", "asdf"},
  };

  for (const auto& params : tinfo_params) {
    inspector->add_thread(build_threadinfo(params));
  }

  auto lineage = SensorClientFormatter::GetProcessLineage(inspector->get_thread_ref(4).get());

  EXPECT_STATS_COUNTER(CollectorStats::process_lineage_counts, 1);
  EXPECT_STATS_COUNTER(CollectorStats::process_lineage_total, 1);
  EXPECT_STATS_COUNTER(CollectorStats::process_lineage_sqr_total, 1);
  EXPECT_STATS_COUNTER(CollectorStats::process_lineage_string_total, 4);

  EXPECT_EQ(lineage.size(), 1);

  EXPECT_EQ(lineage[0].parent_uid(), 7);
  EXPECT_EQ(lineage[0].parent_exec_file_path(), "asdf");

  CollectorStats::Reset();
}

TEST_F(SensorClientFormatterTest, ProcessCollapseParentChildWithSameNameTest) {
  // {pid, tid, ptid, vpid, uid, container_id, exepath},
  std::vector<ThreadInfoParams> tinfo_params = {
      {3, 3, -1, 1, 42, "", "asdf"},
      {1, 1, 3, 2, 7, "", "asdf"},
      {4, 4, 1, 9, 8, "", "asdf"},
      {5, 5, 4, 10, 9, "", "qwerty"},
  };

  for (const auto& params : tinfo_params) {
    inspector->add_thread(build_threadinfo(params));
  }

  auto lineage = SensorClientFormatter::GetProcessLineage(inspector->get_thread_ref(5).get());

  EXPECT_STATS_COUNTER(CollectorStats::process_lineage_counts, 1);
  EXPECT_STATS_COUNTER(CollectorStats::process_lineage_total, 1);
  EXPECT_STATS_COUNTER(CollectorStats::process_lineage_sqr_total, 1);
  EXPECT_STATS_COUNTER(CollectorStats::process_lineage_string_total, 4);

  EXPECT_EQ(lineage.size(), 1);

  EXPECT_EQ(lineage[0].parent_uid(), 8);
  EXPECT_EQ(lineage[0].parent_exec_file_path(), "asdf");

  CollectorStats::Reset();
}

TEST_F(SensorClientFormatterTest, ProcessCollapseParentChildWithSameName2Test) {
  // {pid, tid, ptid, vpid, uid, container_id, exepath},
  std::vector<ThreadInfoParams> tinfo_params = {
      {3, 3, -1, 1, 42, "", "qwerty"},
      {1, 1, 3, 2, 7, "", "asdf"},
      {4, 4, 1, 9, 8, "", "asdf"},
      {5, 5, 4, 10, 9, "", "asdf"},
  };

  for (const auto& params : tinfo_params) {
    inspector->add_thread(build_threadinfo(params));
  }
  auto lineage = SensorClientFormatter::GetProcessLineage(inspector->get_thread_ref(5).get());

  EXPECT_STATS_COUNTER(CollectorStats::process_lineage_counts, 1);
  EXPECT_STATS_COUNTER(CollectorStats::process_lineage_total, 2);
  EXPECT_STATS_COUNTER(CollectorStats::process_lineage_sqr_total, 4);
  EXPECT_STATS_COUNTER(CollectorStats::process_lineage_string_total, 10);

  EXPECT_EQ(lineage.size(), 2);

  EXPECT_EQ(lineage[0].parent_uid(), 8);
  EXPECT_EQ(lineage[0].parent_exec_file_path(), "asdf");

  EXPECT_EQ(lineage[1].parent_uid(), 42);
  EXPECT_EQ(lineage[1].parent_exec_file_path(), "qwerty");

  CollectorStats::Reset();
}

TEST_F(SensorClientFormatterTest, ProcessWithUnrelatedProcessTest) {
  // {pid, tid, ptid, vpid, uid, container_id, exepath},
  std::vector<ThreadInfoParams> tinfo_params = {
      {3, 3, -1, 1, 42, "", "qwerty"},
      {1, 1, 3, 2, 7, "", "asdf"},
      {4, 4, 1, 9, 8, "", "uiop"},
      {5, 5, 555, 10, 9, "", "jkl;"},
  };

  for (const auto& params : tinfo_params) {
    inspector->add_thread(build_threadinfo(params));
  }

  auto lineage = SensorClientFormatter::GetProcessLineage(inspector->get_thread_ref(4).get());

  EXPECT_STATS_COUNTER(CollectorStats::process_lineage_counts, 1);
  EXPECT_STATS_COUNTER(CollectorStats::process_lineage_total, 2);
  EXPECT_STATS_COUNTER(CollectorStats::process_lineage_sqr_total, 4);
  EXPECT_STATS_COUNTER(CollectorStats::process_lineage_string_total, 10);

  EXPECT_EQ(lineage.size(), 2);

  EXPECT_EQ(lineage[0].parent_uid(), 7);
  EXPECT_EQ(lineage[0].parent_exec_file_path(), "asdf");

  EXPECT_EQ(lineage[1].parent_uid(), 42);
  EXPECT_EQ(lineage[1].parent_exec_file_path(), "qwerty");

  CollectorStats::Reset();
}

TEST_F(SensorClientFormatterTest, CountTwoCounterCallsTest) {
  // {pid, tid, ptid, vpid, uid, container_id, exepath},
  inspector->add_thread(build_threadinfo({1, 1, 555, 10, 9, "", "jkl;"}));
  auto lineage = SensorClientFormatter::GetProcessLineage(inspector->get_thread_ref(1).get());

  // {pid, tid, ptid, vpid, uid, container_id, exepath},
  inspector->add_thread(build_threadinfo({2, 2, 555, 10, 9, "", "jkl;"}));
  auto lineage2 = SensorClientFormatter::GetProcessLineage(inspector->get_thread_ref(2).get());

  EXPECT_STATS_COUNTER(CollectorStats::process_lineage_counts, 2);
  EXPECT_STATS_COUNTER(CollectorStats::process_lineage_total, 0);
  EXPECT_STATS_COUNTER(CollectorStats::process_lineage_sqr_total, 0);
  EXPECT_STATS_COUNTER(CollectorStats::process_lineage_string_total, 0);

  EXPECT_TRUE(lineage2.empty());

  CollectorStats::Reset();
}

TEST_F(SensorClientFormatterTest, ProcessArguments) {
  // {pid, tid, ptid, vpid, uid, container_id, exepath},
  auto tinfo = build_threadinfo({3, 3, -1, 0, 42, "", "qwerty"});

  std::vector<std::string> args = {std::string("args")};
  tinfo->set_args(args);

  std::unique_ptr<sinsp_evt> evt(new sinsp_evt());
  std::unique_ptr<scap_evt> s_evt(new scap_evt());

  s_evt->type = PPME_SYSCALL_EXECVE_19_X;
  evt->set_tinfo(tinfo.get());
  evt->set_scap_evt(s_evt.get());

  auto* signal = formatter.CreateProcessSignal(evt.get());
  EXPECT_FALSE(signal->args().empty());
}

TEST_F(SensorClientFormatterTest, NoProcessArguments) {
  config.disable_process_arguments_ = true;

  // {pid, tid, ptid, vpid, uid, container_id, exepath},
  auto tinfo = build_threadinfo({3, 3, -1, 0, 42, "", "qwerty"});

  std::vector<std::string> args = {std::string("args")};
  tinfo->set_args(args);

  std::unique_ptr<sinsp_evt> evt(new sinsp_evt());
  std::unique_ptr<scap_evt> s_evt(new scap_evt());

  s_evt->type = PPME_SYSCALL_EXECVE_19_X;
  evt->set_tinfo(tinfo.get());
  evt->set_scap_evt(s_evt.get());

  auto* signal = formatter.CreateProcessSignal(evt.get());
  EXPECT_TRUE(signal->args().empty());
}

}  // namespace collector
