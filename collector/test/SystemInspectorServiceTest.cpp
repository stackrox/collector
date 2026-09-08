#include <cstdlib>

#include <libsinsp/filter.h>
#include <libsinsp/plugin.h>
#include <libsinsp/sinsp.h>

#include "Utility.h"
#include "gtest/gtest.h"
#include "system-inspector/Service.h"

namespace collector::system_inspector {

TEST(SystemInspectorServiceTest, FilterEvent) {
  std::unique_ptr<sinsp> inspector(new sinsp());
  const char* plugin_path = std::getenv("ROX_COLLECTOR_CONTAINER_PLUGIN_PATH");
  ASSERT_NE(plugin_path, nullptr);
  auto plugin = inspector->register_plugin(plugin_path);
  std::string error;
  ASSERT_TRUE(plugin->init("{}", error)) << error;
  sinsp_filter_check_list filter_list;
  filter_list.add_filter_check(inspector->new_generic_filtercheck());
  filter_list.add_filter_check(sinsp_plugin::new_filtercheck(plugin));
  auto filter_factory = std::make_shared<sinsp_filter_factory>(inspector.get(), filter_list);
  sinsp_filter_compiler filter_compiler(filter_factory, "container.id != host");
  ASSERT_NO_THROW(filter_compiler.compile());

  const auto& fields = inspector->m_thread_manager->dynamic_fields()->fields();
  const auto container_id_field = fields.find("container_id");
  ASSERT_NE(container_id_field, fields.end());
  const auto container_id_accessor = container_id_field->second.new_accessor<std::string>();
  const auto& factory = inspector->get_threadinfo_factory();

  auto regular_process = factory.create();
  regular_process->m_exepath = "/bin/busybox";
  regular_process->m_comm = "sleep";
  regular_process->set_dynamic_field(container_id_accessor, std::string("aaaaaaaaaaaa"));

  auto runc_process = factory.create();
  runc_process->m_exepath = "runc";
  runc_process->m_comm = "6";

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
      {runc_process.get(), true, "runc process"},
      {host_process.get(), true, "host process"},
  };

  for (const auto& t : tests) {
    ASSERT_EQ(system_inspector::Service::FilterEvent(*inspector, t.tinfo), t.expected)
        << "Failed for: " << t.name;
  }

  EXPECT_EQ(GetContainerID(*inspector, *regular_process), "aaaaaaaaaaaa");
  regular_process->set_dynamic_field(container_id_accessor, std::string("host"));
  EXPECT_TRUE(GetContainerID(*inspector, *regular_process).empty());
}

}  // namespace collector::system_inspector
