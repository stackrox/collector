#include <libsinsp/sinsp.h>

#include "gtest/gtest.h"
#include "system-inspector/Service.h"

namespace collector::system_inspector {

TEST(SystemInspectorServiceTest, FilterEvent) {
  std::unique_ptr<sinsp> inspector(new sinsp());
  const auto& factory = inspector->get_threadinfo_factory();

  // A container cgroup path with a 64-hex-char container ID.
  const std::string container_cgroup = "/kubepods/burstable/pod123/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
  sinsp_threadinfo::cgroups_t container_cgroups = {{"memory", container_cgroup}};

  auto regular_process = factory.create();
  regular_process->m_exepath = "/bin/busybox";
  regular_process->m_comm = "sleep";
  regular_process->set_cgroups(container_cgroups);

  auto runc_process = factory.create();
  runc_process->m_exepath = "runc";
  runc_process->m_comm = "6";

  auto proc_self_process = factory.create();
  proc_self_process->m_exepath = "/proc/self/exe";
  proc_self_process->m_comm = "6";
  proc_self_process->set_cgroups(container_cgroups);

  auto memfd_process = factory.create();
  memfd_process->m_exepath = "memfd:runc_cloned:/proc/self/exe";
  memfd_process->m_comm = "6";
  memfd_process->set_cgroups(container_cgroups);

  auto host_process = factory.create();
  host_process->m_exepath = "/usr/bin/bash";
  host_process->m_comm = "bash";

  struct test_t {
    const sinsp_threadinfo* tinfo;
    bool expected;
    const char* name;
  };
  std::vector<test_t> tests{
      {regular_process.get(), true, "regular container process"},
      {runc_process.get(), false, "runc (no container ID)"},
      {proc_self_process.get(), false, "/proc/self exe path"},
      {memfd_process.get(), false, "memfd /proc/self exe path"},
      {host_process.get(), false, "host process (no container ID)"},
  };

  for (const auto& t : tests) {
    ASSERT_EQ(system_inspector::Service::FilterEvent(t.tinfo), t.expected)
        << "Failed for: " << t.name;
  }
}

}  // namespace collector::system_inspector
