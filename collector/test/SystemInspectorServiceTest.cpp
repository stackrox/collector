#include <libsinsp/sinsp.h>

#include "gtest/gtest.h"
#include "system-inspector/Service.h"

namespace collector::system_inspector {

TEST(SystemInspectorServiceTest, FilterEvent) {
  std::unique_ptr<sinsp> inspector(new sinsp());
  const auto& factory = inspector->get_threadinfo_factory();

  auto regular_process = factory.create();
  regular_process->m_exepath = "/bin/busybox";
  regular_process->m_comm = "sleep";

  auto runc_process = factory.create();
  runc_process->m_exepath = "runc";
  runc_process->m_comm = "6";

  auto proc_self_process = factory.create();
  proc_self_process->m_exepath = "/proc/self/exe";
  proc_self_process->m_comm = "6";

  auto memfd_process = factory.create();
  memfd_process->m_exepath = "memfd:runc_cloned:/proc/self/exe";
  memfd_process->m_comm = "6";

  struct test_t {
    const sinsp_threadinfo* tinfo;
    bool expected;
  };
  std::vector<test_t> tests{
      {regular_process.get(), true},
      {runc_process.get(), false},
      {proc_self_process.get(), false},
      {memfd_process.get(), false},
  };

  for (const auto& t : tests) {
    ASSERT_EQ(system_inspector::Service::FilterEvent(t.tinfo), t.expected);
  }
}

}  // namespace collector::system_inspector
