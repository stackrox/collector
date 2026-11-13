#include <libsinsp/filter.h>
#include <libsinsp/sinsp.h>

#include "CollectorStats.h"
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
  ASSERT_EQ(container_id_cache.Get(*regular_process), "aaaaaaaaaaaa");

  auto runc_process = factory.create();
  runc_process->m_tid = 2;
  runc_process->m_exepath = "runc";
  runc_process->m_comm = "6";

  auto host_process = factory.create();
  host_process->m_tid = 3;
  host_process->m_exepath = "/usr/bin/bash";
  host_process->m_comm = "bash";

  auto pid_namespace_process = factory.create();
  pid_namespace_process->m_tid = 42;
  pid_namespace_process->m_vtid = 1;

  auto uncached_container_process = factory.create();
  uncached_container_process->m_tid = 43;
  uncached_container_process->set_cgroups({"cpu=/docker/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"});

  auto proc_self_process = factory.create();
  proc_self_process->m_tid = 4;
  proc_self_process->m_exepath = "container:/proc/self/exe";

  auto memfd_process = factory.create();
  memfd_process->m_tid = 5;
  memfd_process->m_exepath = "/memfd:payload (deleted)";

  auto runc_cloned_process = factory.create();
  runc_cloned_process->m_tid = 6;
  runc_cloned_process->m_exepath = "memfd:runc_cloned:/proc/self/exe";
  runc_cloned_process->set_cgroups({"cpu=/docker/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"});

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
      {runc_cloned_process.get(), false, "runc-cloned memfd /proc/self exe path"},
  };

  // The static FilterEvent only enforces the exepath rule; the container-ID
  // rejection lives in the production instance guard asserted further below.
  for (const auto& t : tests) {
    ASSERT_EQ(system_inspector::Service::FilterEvent(t.tinfo), t.expected)
        << "Failed for: " << t.name;
  }

  event.set_tinfo(regular_process.get());
  EXPECT_TRUE(filter->run(&event));
  event.set_tinfo(runc_process.get());
  EXPECT_FALSE(filter->run(&event));
  event.set_tinfo(host_process.get());
  EXPECT_FALSE(filter->run(&event));
  event.set_tinfo(pid_namespace_process.get());
  EXPECT_TRUE(filter->run(&event));

  // Service::FilterEvent(sinsp_evt*) is the production guard run by GetNext:
  // an event passes only when its thread resolves to a non-empty cached
  // container ID (rejecting host processes, runtime helpers, and
  // PID-namespace threads without a container ID) and survives the exepath
  // rule asserted above. The event's thread info belongs to the test
  // inspector; the service consults its own cache, which populates lazily on
  // the first lookup.
  CollectorConfig config;
  Service service(config);
  event.set_tinfo(host_process.get());
  EXPECT_FALSE(service.FilterEvent(&event));
  event.set_tinfo(runc_process.get());
  EXPECT_FALSE(service.FilterEvent(&event));
  event.set_tinfo(pid_namespace_process.get());
  EXPECT_FALSE(service.FilterEvent(&event));
  event.set_tinfo(regular_process.get());
  EXPECT_TRUE(service.FilterEvent(&event));
  event.set_tinfo(runc_cloned_process.get());
  EXPECT_FALSE(service.FilterEvent(&event));
  service.CleanUp();

  EXPECT_EQ(container_id_cache.Get(*uncached_container_process), "aaaaaaaaaaaa");
}

TEST(SystemInspectorServiceTest, ContainerIDCacheLookupStats) {
  std::unique_ptr<sinsp> inspector(new sinsp());
  ContainerIDCache container_id_cache;
  COUNTER_ZERO(CollectorStats::container_id_cache_hits);
  COUNTER_ZERO(CollectorStats::container_id_cache_misses);
  COUNTER_ZERO(CollectorStats::container_id_cache_empty_extractions);
  auto tinfo = inspector->get_threadinfo_factory().create();
  tinfo->m_tid = 42;
  tinfo->m_clone_ts = 100;
  tinfo->set_cgroups({"cpu=/docker/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"});

  EXPECT_EQ(container_id_cache.Get(*tinfo), "aaaaaaaaaaaa");
  EXPECT_EQ(container_id_cache.Get(*tinfo), "aaaaaaaaaaaa");

  ++tinfo->m_clone_ts;
  EXPECT_EQ(container_id_cache.Get(*tinfo), "aaaaaaaaaaaa");

  // Threads without a container ID are never cached, so their lookups
  // recompute (and count as misses) on every event.
  auto host_tinfo = inspector->get_threadinfo_factory().create();
  host_tinfo->m_tid = 43;
  EXPECT_EQ(container_id_cache.Get(*host_tinfo), "");
  EXPECT_EQ(container_id_cache.Get(*host_tinfo), "");

  EXPECT_EQ(CollectorStats::GetOrCreate().GetCounter(CollectorStats::container_id_cache_hits), 1);
  EXPECT_EQ(CollectorStats::GetOrCreate().GetCounter(CollectorStats::container_id_cache_misses), 4);
  EXPECT_EQ(CollectorStats::GetOrCreate().GetCounter(CollectorStats::container_id_cache_empty_extractions), 2);
  COUNTER_ZERO(CollectorStats::container_id_cache_hits);
  COUNTER_ZERO(CollectorStats::container_id_cache_misses);
  COUNTER_ZERO(CollectorStats::container_id_cache_empty_extractions);
}

TEST(SystemInspectorServiceTest, ContainerIDCacheLateCgroups) {
  std::unique_ptr<sinsp> inspector(new sinsp());
  ContainerIDCache container_id_cache;
  auto tinfo = inspector->get_threadinfo_factory().create();
  tinfo->m_tid = 42;
  tinfo->m_clone_ts = 100;

  // A thread first seen before its cgroups are known (e.g. a dropped clone
  // event) has no container ID to cache.
  EXPECT_EQ(container_id_cache.Get(*tinfo), "");

  // When the cgroups arrive (libsinsp populates them when it (re)creates or
  // refreshes the thread info, e.g. from an event permitting a /proc query or
  // clone/execve exit parsing), the next lookup must pick up the container ID
  // instead of pinning the empty result.
  tinfo->set_cgroups({"cpu=/docker/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"});
  EXPECT_EQ(container_id_cache.Get(*tinfo), "aaaaaaaaaaaa");
  EXPECT_EQ(container_id_cache.Get(*tinfo), "aaaaaaaaaaaa");
}

TEST(SystemInspectorServiceTest, ContainerIDCacheRefreshesAfterExec) {
  std::unique_ptr<sinsp> inspector(new sinsp());
  ContainerIDCache container_id_cache;
  auto tinfo = inspector->get_threadinfo_factory().create();
  tinfo->m_tid = 42;
  tinfo->m_clone_ts = 100;
  tinfo->m_lastexec_ts = 200;
  tinfo->set_cgroups({"cpu=/docker/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"});

  EXPECT_EQ(container_id_cache.Get(*tinfo), "aaaaaaaaaaaa");
  EXPECT_EQ(container_id_cache.Get(*tinfo), "aaaaaaaaaaaa");

  // Exec parsing can refresh a thread's cgroups without changing its TID or
  // clone timestamp. The cache must not keep returning the previous ID.
  tinfo->set_cgroups({"cpu=/docker/bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"});
  ++tinfo->m_lastexec_ts;

  EXPECT_EQ(container_id_cache.Get(*tinfo), "bbbbbbbbbbbb");
  EXPECT_EQ(container_id_cache.Get(*tinfo), "bbbbbbbbbbbb");
}

TEST(SystemInspectorServiceTest, ContainerIDCachePrune) {
  std::unique_ptr<sinsp> inspector(new sinsp());
  ContainerIDCache container_id_cache;
  EXPECT_EQ(container_id_cache.Size(), 0);
  COUNTER_ZERO(CollectorStats::container_id_cache_hits);
  COUNTER_ZERO(CollectorStats::container_id_cache_misses);
  COUNTER_ZERO(CollectorStats::container_id_cache_empty_extractions);
  auto tinfo = inspector->get_threadinfo_factory().create();
  tinfo->m_pid = 42;
  tinfo->m_tid = 42;
  tinfo->m_ptid = -1;
  tinfo->m_clone_ts = 100;
  tinfo->set_cgroups({"cpu=/docker/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"});

  // The thread is not in the thread table: the first lookup extracts and
  // caches the container ID.
  EXPECT_EQ(container_id_cache.Get(*tinfo), "aaaaaaaaaaaa");
  EXPECT_EQ(container_id_cache.Size(), 1);
  EXPECT_EQ(container_id_cache.Get(*tinfo), "aaaaaaaaaaaa");

  // The first prune (last_prune_us_ starts at 0, so 61s clears the 60s rate
  // limit) evicts the entry, because the tid is not in the thread table.
  container_id_cache.Prune(*inspector, 61'000'000);
  EXPECT_EQ(container_id_cache.Size(), 0);
  EXPECT_EQ(container_id_cache.Get(*tinfo), "aaaaaaaaaaaa");
  EXPECT_EQ(container_id_cache.Size(), 1);
  EXPECT_EQ(container_id_cache.Get(*tinfo), "aaaaaaaaaaaa");

  // Only 30s since the last prune: rate-limited no-op, the entry survives.
  container_id_cache.Prune(*inspector, 91'000'000);
  EXPECT_EQ(container_id_cache.Get(*tinfo), "aaaaaaaaaaaa");

  // A thread present in the thread table counts as live.
  inspector->m_thread_manager->add_thread(std::move(tinfo), false);

  // 61s since the last real prune: prunes, but the tid is now live, so the
  // entry survives.
  container_id_cache.Prune(*inspector, 122'000'000);
  EXPECT_EQ(container_id_cache.Get(*inspector->m_thread_manager->find_thread(42, true)), "aaaaaaaaaaaa");

  EXPECT_EQ(CollectorStats::GetOrCreate().GetCounter(CollectorStats::container_id_cache_hits), 4);
  EXPECT_EQ(CollectorStats::GetOrCreate().GetCounter(CollectorStats::container_id_cache_misses), 2);
  EXPECT_EQ(CollectorStats::GetOrCreate().GetCounter(CollectorStats::container_id_cache_empty_extractions), 0);
  COUNTER_ZERO(CollectorStats::container_id_cache_hits);
  COUNTER_ZERO(CollectorStats::container_id_cache_misses);
  COUNTER_ZERO(CollectorStats::container_id_cache_empty_extractions);
}

}  // namespace collector::system_inspector
