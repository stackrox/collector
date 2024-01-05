#include "SysdigService.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace collector {

TEST(SysdigServiceTest, FilterEvent) {
  std::unique_ptr<sinsp> inspector(new sinsp());

  sinsp_threadinfo regular_process(inspector.get());
  regular_process.m_exepath = "/bin/busybox";
  regular_process.m_comm = "sleep";

  sinsp_threadinfo runc_process(inspector.get());
  runc_process.m_exepath = "runc";
  runc_process.m_comm = "6";

  sinsp_threadinfo proc_self_process(inspector.get());
  proc_self_process.m_exepath = "/proc/self/exe";
  proc_self_process.m_comm = "6";

  sinsp_threadinfo memfd_process(inspector.get());
  memfd_process.m_exepath = "memfd:runc_cloned:/proc/self/exe";
  memfd_process.m_comm = "6";

  struct test_t {
    const sinsp_threadinfo* tinfo;
    bool expected;
  };
  std::vector<test_t> tests{
      {&regular_process, true},
      {&runc_process, false},
      {&proc_self_process, false},
      {&memfd_process, false},
  };

  for (const auto& t : tests) {
    ASSERT_EQ(SysdigService::FilterEvent(t.tinfo), t.expected);
  }
}

}  // namespace collector
