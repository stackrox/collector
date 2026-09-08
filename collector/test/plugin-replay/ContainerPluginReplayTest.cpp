#include <cstdlib>
#include <cstring>
#include <memory>
#include <sinsp_with_test_input.h>
#include <string>
#include <vector>

#include <libsinsp/filter.h>
#include <libsinsp/plugin.h>

// The upstream fixture macro collides with protobuf's ErrorLocation enum.
#undef DEFAULT_VALUE

#include "Corpus.h"
#include "NetworkSignalHandler.h"
#include "Utility.h"

namespace collector {
namespace {

const std::string kID(64, 'a');
const std::string kPath = "/kubepods/burstable/pod123/" + kID;
const std::string kShortID = kID.substr(0, 12);

class ContainerPluginReplayTest : public sinsp_with_test_input {
 protected:
  void SetUp() override {
    const char* path = std::getenv("ROX_COLLECTOR_CONTAINER_PLUGIN_PATH");
    ASSERT_NE(path, nullptr);
    plugin_ = m_inspector.register_plugin(path);
    std::string error;
    ASSERT_TRUE(plugin_->init("{}", error)) << error;
  }

  void SeedThread(int64_t tid, int64_t vpid,
                  const std::vector<std::string>& cgroups) {
    auto thread = create_threadinfo(tid, tid, 0, tid, vpid, vpid,
                                    "replay", "/bin/replay", "/bin/replay",
                                    increasing_ts(), 0, 0, {}, 0, {}, "/");
    const auto bytes = test_utils::to_null_delimited(cgroups);
    ASSERT_LE(bytes.size(), sizeof(thread.cgroups.path));
    std::memcpy(thread.cgroups.path, bytes.data(), bytes.size());
    thread.cgroups.len = bytes.size();
    add_thread(thread, {});
  }

  void Open() {
    open_inspector();  // Falco invokes plugin_capture_open; tests never do so directly.
    sinsp_filter_check_list checks;
    checks.add_filter_check(m_inspector.new_generic_filtercheck());
    checks.add_filter_check(sinsp_plugin::new_filtercheck(plugin_));
    auto factory = std::make_shared<sinsp_filter_factory>(&m_inspector, checks);
    sinsp_filter_compiler compiler(factory, "container.id != host");
    filter_ = compiler.compile();
  }

  void ExpectAttribution(int64_t tid, const std::string& expected, bool accepted) {
    auto* event = generate_random_event(tid);
    ASSERT_NE(event, nullptr);
    EXPECT_EQ(GetContainerID(event), expected);
    EXPECT_EQ(filter_->run(event), accepted);
  }

  void ExpectConnection(int64_t tid, const std::string& expected, bool accepted) {
    ASSERT_NE(generate_socket_exit_event({}, tid), nullptr);
    auto* event = generate_connect_exit_event({}, tid);
    ASSERT_NE(event, nullptr);
    ASSERT_NE(event->get_thread_info(), nullptr);
    ASSERT_FALSE(event->get_thread_info()->is_invalid());
    ASSERT_NE(event->get_fd_info(), nullptr);
    EXPECT_EQ(GetContainerID(event), expected);
    EXPECT_EQ(filter_->run(event), accepted);
  }

  void ImportLateThread(int64_t tid, const std::vector<std::string>& cgroups) {
    // Models a successful late /proc lookup by importing its resulting state.
    // TEST_INPUT has no proc_get callback; this is not an actual /proc lookup.
    auto thread = m_inspector.get_threadinfo_factory().create();
    thread->m_tid = tid;
    thread->m_pid = tid;
    thread->m_ptid = 0;
    thread->m_vtid = tid;
    thread->m_vpid = tid;
    thread->m_clone_ts = increasing_ts();
    thread->m_comm = "late-import";
    thread->m_exepath = "/bin/late-import";
    thread->set_cgroups(cgroups);
    ASSERT_NE(m_inspector.m_thread_manager->add_thread(std::move(thread), false), nullptr);
  }

  void ExpectHostPIDChildConnectionTracked(bool include_child_event) {
    SeedThread(200, 200, {"memory=" + kPath});
    Open();
    ASSERT_NE(generate_clone_x_event(201, 200, 200, 0, 0, 200, 200,
                                     "parent", {"memory=" + kPath}, PPME_SYSCALL_FORK_20_X),
              nullptr);
    if (include_child_event) {
      ASSERT_NE(generate_clone_x_event(0, 201, 201, 200, 0, 201, 201,
                                       "child", {"memory=" + kPath}, PPME_SYSCALL_FORK_20_X),
                nullptr);
    }
    auto tracker = std::make_shared<ConnectionTracker>();
    system_inspector::Stats stats;
    NetworkSignalHandler handler(&m_inspector, tracker, &stats);
    ASSERT_NE(generate_socket_exit_event({}, 201), nullptr);
    auto* event = generate_connect_exit_event({}, 201);
    ASSERT_NE(event, nullptr);
    ASSERT_TRUE(filter_->run(event));
    EXPECT_EQ(handler.HandleSignal(event), SignalHandler::PROCESSED);
    const auto connections = tracker->FetchConnState(false, false);
    ASSERT_EQ(connections.size(), 1U);
    EXPECT_EQ(connections.begin()->first.container(), kShortID);
  }

  std::shared_ptr<sinsp_plugin> plugin_;
  std::unique_ptr<sinsp_filter> filter_;
};

TEST_F(ContainerPluginReplayTest, StartupHostContainerAndHostPID) {
  SeedThread(100, 100, {"memory=/"});
  SeedThread(200, 1, {"memory=" + kPath});
  SeedThread(300, 300, {"memory=" + kPath});
  Open();
  ExpectAttribution(100, "", false);
  ExpectAttribution(200, kShortID, true);
  ExpectAttribution(300, kShortID, true);
}

TEST_F(ContainerPluginReplayTest, MatchingCgroupBeforeNonmatch) {
  SeedThread(200, 1, {"memory=" + kPath, "cpuset=/"});
  Open();
  ExpectAttribution(200, kShortID, true);
}

TEST_F(ContainerPluginReplayTest, MatchingCgroupAfterNonmatch) {
  SeedThread(200, 1, {"cpuset=/", "memory=" + kPath});
  Open();
  ExpectAttribution(200, kShortID, true);
}

TEST_F(ContainerPluginReplayTest, InstalledFilterRejectsHostAndAcceptsContainer) {
  SeedThread(100, 100, {"memory=/"});
  SeedThread(200, 1, {"memory=" + kPath});
  Open();
  m_inspector.set_filter(std::move(filter_), "container.id != host");
  add_filtered_event_advance_ts(increasing_ts(), 100, PPME_SOCKET_GETSOCKNAME_X, 0);
  auto* event = generate_random_event(200);
  ASSERT_NE(event, nullptr);
  EXPECT_EQ(GetContainerID(event), kShortID);
}

TEST_F(ContainerPluginReplayTest, ForkWithoutExecAttributesFirstConnection) {
  SeedThread(200, 1, {"memory=" + kPath});
  Open();
  // Replay parent and child fork exits; no exec event is generated.
  ASSERT_NE(generate_clone_x_event(201, 200, 200, 0, 0, 1, 1,
                                   "parent", {"memory=" + kPath},
                                   PPME_SYSCALL_FORK_20_X),
            nullptr);
  ASSERT_NE(generate_clone_x_event(0, 201, 201, 200, 0, 2, 2,
                                   "child", {"memory=" + kPath},
                                   PPME_SYSCALL_FORK_20_X),
            nullptr);
  ASSERT_NE(generate_socket_exit_event({}, 201), nullptr);
  auto* event = generate_connect_exit_event({}, 201);
  ASSERT_NE(event, nullptr);
  ASSERT_NE(event->get_fd_info(), nullptr);
  EXPECT_EQ(GetContainerID(event), kShortID);
  EXPECT_TRUE(filter_->run(event));
}

class StartupCorpusTest : public ContainerPluginReplayTest,
                          public ::testing::WithParamInterface<replay_corpus::StartupCase> {};

TEST_P(StartupCorpusTest, AttributionAndFilter) {
  const auto& scenario = GetParam();
  SeedThread(200, 200, scenario.cgroups);
  Open();
  ExpectAttribution(200, scenario.expected_id, !scenario.expected_id.empty());
}

INSTANTIATE_TEST_SUITE_P(Corpus, StartupCorpusTest,
                         ::testing::ValuesIn(replay_corpus::StartupCases()),
                         [](const auto& info) { return info.param.name; });

class ForkCorpusTest : public ContainerPluginReplayTest,
                       public ::testing::WithParamInterface<replay_corpus::ForkCase> {};

TEST_P(ForkCorpusTest, FirstConnectionAttributionAndFilter) {
  using namespace replay_corpus;
  const auto& scenario = GetParam();
  const bool container = scenario.origin != Origin::Host;
  const bool pidns = scenario.origin == Origin::PIDNamespaceContainer;
  const std::vector<std::string> cgroups = {container ? "memory=" + kPath : "memory=/"};
  SeedThread(200, pidns ? 1 : 200, cgroups);
  Open();
  const uint32_t flags = pidns ? PPM_CL_CHILD_IN_PIDNS : 0;
  auto parent = [&]() {
    ASSERT_NE(generate_clone_x_event(pidns ? 2 : 201, 200, 200, 0, flags,
                                     pidns ? 1 : 200, pidns ? 1 : 200,
                                     "parent", cgroups, scenario.event_type),
              nullptr);
  };
  auto child = [&]() {
    ASSERT_NE(generate_clone_x_event(0, 201, 201, 200, flags,
                                     pidns ? 2 : 201, pidns ? 2 : 201,
                                     "child", cgroups, scenario.event_type),
              nullptr);
  };
  switch (scenario.order) {
    case ForkOrder::ParentThenChild:
      parent();
      child();
      break;
    case ForkOrder::ChildThenParent:
      child();
      parent();
      break;
    case ForkOrder::ChildOnly:
      child();
      break;
    case ForkOrder::ParentOnly:
      parent();
      break;
  }
  // Prove Falco already has enough information; missing attribution here cannot
  // be excused by a missing thread or missing cgroup payload in the scenario.
  auto thread = m_inspector.m_thread_manager->find_thread(201, true);
  ASSERT_NE(thread, nullptr);
  ASSERT_FALSE(thread->is_invalid());
  ASSERT_FALSE(thread->cgroups().empty());
  EXPECT_EQ(thread->cgroups().front().second, container ? kPath : "/");
  const auto& field = m_inspector.m_thread_manager->dynamic_fields()->fields().at("container_id");
  std::string cached_id;
  thread->get_dynamic_field(field.new_accessor<std::string>(), cached_id);
  RecordProperty("cached_id_before_connection", cached_id);
  RecordProperty("child_cgroup", thread->cgroups().front().second);
  ExpectConnection(201, container ? kShortID : "", container);
}

INSTANTIATE_TEST_SUITE_P(Corpus, ForkCorpusTest,
                         ::testing::ValuesIn(replay_corpus::ForkCases()),
                         [](const auto& info) { return info.param.name; });

TEST_F(ContainerPluginReplayTest, LateImportedHostIsRejected) {
  SeedThread(1, 1, {"memory=/"});
  Open();
  ImportLateThread(200, {"memory=/"});
  ExpectConnection(200, "", false);
}

TEST_F(ContainerPluginReplayTest, LateImportedContainerIsAttributed) {
  SeedThread(1, 1, {"memory=/"});
  Open();
  ImportLateThread(200, {"memory=" + kPath});
  ExpectConnection(200, kShortID, true);
}

TEST_F(ContainerPluginReplayTest, ParentForkHostChildRejectedByInstalledFilter) {
  SeedThread(200, 200, {"memory=/"});
  Open();
  ASSERT_NE(generate_clone_x_event(201, 200, 200, 0, 0, 200, 200,
                                   "parent", {"memory=/"}, PPME_SYSCALL_FORK_20_X),
            nullptr);
  auto child = m_inspector.m_thread_manager->find_thread(201, true);
  ASSERT_NE(child, nullptr);
  ASSERT_FALSE(child->is_invalid());
  m_inspector.set_filter(std::move(filter_), "container.id != host");
  // The child-side fork event was dropped; this is a socket event on the host.
  add_filtered_event_advance_ts(increasing_ts(), 201, PPME_SOCKET_GETSOCKNAME_X, 0);
}

TEST_F(ContainerPluginReplayTest, ChildForkRepairsPreviouslyUncachedHostPIDChild) {
  SeedThread(200, 200, {"memory=" + kPath});
  Open();
  ASSERT_NE(generate_clone_x_event(201, 200, 200, 0, 0, 200, 200,
                                   "parent", {"memory=" + kPath}, PPME_SYSCALL_FORK_20_X),
            nullptr);
  // Child traffic may precede the observed child-side event. Do not assert it
  // here: the parent-only corpus separately checks that failing interval.
  ASSERT_NE(generate_socket_exit_event({}, 201), nullptr);
  ASSERT_NE(generate_clone_x_event(0, 201, 201, 200, 0, 201, 201,
                                   "child", {"memory=" + kPath}, PPME_SYSCALL_FORK_20_X),
            nullptr);
  ExpectConnection(201, kShortID, true);
}

TEST_F(ContainerPluginReplayTest, ExecRefreshesContainerAttribution) {
  SeedThread(200, 1, {"memory=" + kPath});
  Open();
  ASSERT_NE(generate_execve_enter_and_exit_event(0, 200, 200, 200, 0,
                                                 "/bin/new", "new", "/bin/new", {"memory=/docker/" + replay_corpus::kB}),
            nullptr);
  ExpectConnection(200, "bbbbbbbbbbbb", true);
}

TEST_F(ContainerPluginReplayTest, ExecveatRefreshesContainerAttribution) {
  SeedThread(200, 1, {"memory=" + kPath});
  Open();
  ASSERT_NE(generate_execveat_enter_and_exit_event(0, 200, 200, 200, 0,
                                                   "/bin/new", "new", "/bin/new", {"memory=/docker/" + replay_corpus::kB}),
            nullptr);
  ExpectConnection(200, "bbbbbbbbbbbb", true);
}

TEST_F(ContainerPluginReplayTest, FailedExecPreservesAttribution) {
  SeedThread(200, 1, {"memory=" + kPath});
  Open();
  ASSERT_NE(generate_execve_enter_and_exit_event(-2, 200, 200, 200, 0,
                                                 "/missing", "missing", "/missing", {}),
            nullptr);
  ExpectConnection(200, kShortID, true);
}

TEST_F(ContainerPluginReplayTest, TIDReuseAcrossContainersDoesNotRetainOldID) {
  SeedThread(1, 1, {"memory=/"});
  SeedThread(200, 200, {"memory=" + kPath});
  Open();
  ExpectConnection(200, kShortID, true);
  remove_thread(200, 1);
  ASSERT_EQ(m_inspector.m_thread_manager->find_thread(200, true), nullptr);
  ASSERT_NE(generate_clone_x_event(0, 200, 200, 1, 0, 200, 200,
                                   "replacement", {"memory=/docker/" + replay_corpus::kB}, PPME_SYSCALL_FORK_20_X),
            nullptr);
  ExpectConnection(200, "bbbbbbbbbbbb", true);
}

TEST_F(ContainerPluginReplayTest, TIDReuseFromContainerToHostIsRejected) {
  SeedThread(1, 1, {"memory=/"});
  SeedThread(200, 200, {"memory=" + kPath});
  Open();
  remove_thread(200, 1);
  ASSERT_EQ(m_inspector.m_thread_manager->find_thread(200, true), nullptr);
  ASSERT_NE(generate_clone_x_event(0, 200, 200, 1, 0, 200, 200,
                                   "replacement", {"memory=/"}, PPME_SYSCALL_FORK_20_X),
            nullptr);
  ExpectConnection(200, "", false);
}

TEST_F(ContainerPluginReplayTest, ThreadCloneWithoutExecIsAttributed) {
  SeedThread(200, 1, {"memory=" + kPath});
  Open();
  ASSERT_NE(generate_clone_x_event(0, 201, 200, 0,
                                   PPM_CL_CLONE_THREAD | PPM_CL_CHILD_IN_PIDNS, 2, 1,
                                   "worker", {"memory=" + kPath}, PPME_SYSCALL_CLONE_20_X),
            nullptr);
  ExpectConnection(201, kShortID, true);
}

TEST_F(ContainerPluginReplayTest, VforkChildExitBeforeParentDoesNotResurrectChild) {
  SeedThread(1, 1, {"memory=/"});
  SeedThread(200, 200, {"memory=" + kPath});
  Open();
  ASSERT_NE(generate_clone_x_event(0, 201, 201, 200, PPM_CL_CLONE_VFORK, 201, 201,
                                   "child", {"memory=" + kPath}, PPME_SYSCALL_VFORK_20_X),
            nullptr);
  ExpectConnection(201, kShortID, true);
  remove_thread(201, 200);
  ASSERT_EQ(m_inspector.m_thread_manager->find_thread(201, true), nullptr);
  ASSERT_NE(generate_clone_x_event(201, 200, 200, 0, PPM_CL_CLONE_VFORK, 200, 200,
                                   "parent", {"memory=" + kPath}, PPME_SYSCALL_VFORK_20_X),
            nullptr);
  EXPECT_EQ(m_inspector.m_thread_manager->find_thread(201, true), nullptr);
  ExpectConnection(200, kShortID, true);
}

TEST_F(ContainerPluginReplayTest, HostPIDConnectionTrackedWithBothForkEvents) {
  ExpectHostPIDChildConnectionTracked(true);
}

TEST_F(ContainerPluginReplayTest, HostPIDConnectionTrackedWithoutChildForkEvent) {
  ExpectHostPIDChildConnectionTracked(false);
}

}  // namespace
}  // namespace collector
