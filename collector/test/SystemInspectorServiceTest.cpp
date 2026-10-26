#include <libsinsp/filter.h>
#include <libsinsp/sinsp.h>

#include "CollectorStats.h"
#include "Utility.h"
#include "gtest/gtest.h"
#include "system-inspector/ContainerIDCache.h"
#include "system-inspector/ContainerIDFilterCheck.h"
#include "system-inspector/Service.h"

namespace collector::system_inspector {

TEST(SystemInspectorServiceTest, CleanUpBeforeDestruction) {
  CollectorConfig config;
  {
    Service service(config);
    service.CleanUp();
  }
}

TEST(SystemInspectorServiceTest, FilterEvent) {
  std::unique_ptr<sinsp> inspector(new sinsp());
  ContainerIDCache container_id_cache;
  auto filter = CompileFilter(*inspector, &container_id_cache, "container.id != host");
  const auto& factory = inspector->get_threadinfo_factory();

  auto regular_process = factory.create();
  regular_process->m_tid = 1;
  regular_process->m_exepath = "/bin/busybox";
  regular_process->m_comm = "sleep";
  regular_process->set_cgroups({"cpu=/docker/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"});
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

  auto uncached_container_process = factory.create();
  uncached_container_process->m_tid = 43;
  uncached_container_process->set_cgroups({"cpu=/docker/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"});

  auto proc_self_process = factory.create();
  proc_self_process->m_exepath = "container:/proc/self/exe";

  auto memfd_process = factory.create();
  memfd_process->m_exepath = "/memfd:payload (deleted)";

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
      {proc_self_process.get(), false, "proc self executable"},
      {memfd_process.get(), true, "memfd executable"},
  };

  for (const auto& t : tests) {
    ASSERT_EQ(system_inspector::Service::FilterEvent(t.tinfo), t.expected)
        << "Failed for: " << t.name;
  }

  event.set_tinfo(regular_process.get());
  EXPECT_TRUE(filter->run(&event));
  event.set_tinfo(host_process.get());
  EXPECT_FALSE(filter->run(&event));
  event.set_tinfo(pid_namespace_process.get());
  EXPECT_TRUE(filter->run(&event));
  EXPECT_EQ(container_id_cache.Get(*uncached_container_process), "aaaaaaaaaaaa");
}

TEST(SystemInspectorServiceTest, ContainerIDCacheLookupStats) {
  std::unique_ptr<sinsp> inspector(new sinsp());
  ContainerIDCache container_id_cache;
  COUNTER_ZERO(CollectorStats::container_id_cache_hits);
  COUNTER_ZERO(CollectorStats::container_id_cache_misses);
  auto tinfo = inspector->get_threadinfo_factory().create();
  tinfo->m_tid = 42;
  tinfo->m_clone_ts = 100;
  tinfo->set_cgroups({"cpu=/docker/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"});

  EXPECT_EQ(container_id_cache.Get(*tinfo), "aaaaaaaaaaaa");
  EXPECT_EQ(container_id_cache.Get(*tinfo), "aaaaaaaaaaaa");

  ++tinfo->m_clone_ts;
  EXPECT_EQ(container_id_cache.Get(*tinfo), "aaaaaaaaaaaa");

  EXPECT_EQ(CollectorStats::GetOrCreate().GetCounter(CollectorStats::container_id_cache_hits), 1);
  EXPECT_EQ(CollectorStats::GetOrCreate().GetCounter(CollectorStats::container_id_cache_misses), 2);
  COUNTER_ZERO(CollectorStats::container_id_cache_hits);
  COUNTER_ZERO(CollectorStats::container_id_cache_misses);
}

}  // namespace collector::system_inspector
