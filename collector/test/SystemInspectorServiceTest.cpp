#include <libsinsp/filter.h>
#include <libsinsp/sinsp.h>

#include "Utility.h"
#include "gtest/gtest.h"
#include "system-inspector/ContainerIDCache.h"
#include "system-inspector/ContainerIDFilterCheck.h"
#include "system-inspector/Service.h"

namespace collector::system_inspector {

TEST(SystemInspectorServiceTest, FilterEvent) {
  std::unique_ptr<sinsp> inspector(new sinsp());
  ContainerIDCache container_id_cache;
  sinsp_filter_check_list filter_list;
  filter_list.add_filter_check(inspector->new_generic_filtercheck());

  filter_list.add_filter_check(std::make_unique<ContainerIDFilterCheck>(&container_id_cache));
  auto filter_factory = std::make_shared<sinsp_filter_factory>(inspector.get(), filter_list);
  sinsp_filter_compiler filter_compiler(filter_factory, "container.id != host");
  auto filter = filter_compiler.compile();
  const auto& factory = inspector->get_threadinfo_factory();

  auto regular_process = factory.create();
  regular_process->m_tid = 1;
  regular_process->m_exepath = "/bin/busybox";
  regular_process->m_comm = "sleep";
  regular_process->set_cgroups({"cpu:/docker/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"});
  container_id_cache.on_clone(nullptr, regular_process.get(), -1);

  auto runc_process = factory.create();
  runc_process->m_exepath = "runc";
  runc_process->m_comm = "6";

  auto host_process = factory.create();
  host_process->m_exepath = "/usr/bin/bash";
  host_process->m_comm = "bash";

  auto pid_namespace_process = factory.create();
  pid_namespace_process->m_tid = 42;
  pid_namespace_process->m_vtid = 1;

  sinsp_evt event(inspector.get());

  struct test_t {
    const sinsp_threadinfo* tinfo;
    bool expected;
    const char* name;
  };
  std::vector<test_t> tests{
      {regular_process.get(), true, "regular container process"},
      {runc_process.get(), true, "runc process"},
      {host_process.get(), true, "host process"},
  };

  for (const auto& t : tests) {
    ASSERT_EQ(system_inspector::Service::FilterEvent(*inspector, t.tinfo), t.expected)
        << "Failed for: " << t.name;
  }

  event.set_tinfo(regular_process.get());
  EXPECT_FALSE(filter->run(&event));
  event.set_tinfo(host_process.get());
  EXPECT_FALSE(filter->run(&event));
  event.set_tinfo(pid_namespace_process.get());
  EXPECT_TRUE(filter->run(&event));
}

}  // namespace collector::system_inspector
