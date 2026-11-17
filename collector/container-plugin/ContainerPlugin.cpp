#include <cctype>
#include <cstdint>
#include <ppm_events_public.h>
#include <string>
#include <string_view>

#include <plugin/plugin_api.h>

#include "ContainerID.h"

namespace {

constexpr std::string_view kPluginName = "collector-container";
constexpr std::string_view kPluginVersion = "0.1.0";
constexpr std::string_view kHostContainerID = "host";

struct plugin_state {
  std::string last_error;
  ss_plugin_table_t* threads = nullptr;
  ss_plugin_table_field_t* cgroups = nullptr;
  ss_plugin_table_field_t* cgroup_path = nullptr;
  ss_plugin_table_field_t* container_id = nullptr;
  std::string extracted_container_id;
  const char* extracted_container_id_ptr = nullptr;
};

std::string ContainerIDFromCgroup(std::string_view cgroup) {
  return std::string(collector::container_plugin::ExtractContainerIDFromCgroup(cgroup).value_or(std::string_view{}));
}

struct cgroup_iteration_state {
  plugin_state* plugin;
  ss_plugin_table_reader_vtable_ext* reader;
  ss_plugin_table_t* cgroup_table;
  std::string container_id;
};

ss_plugin_bool FindContainerID(ss_plugin_table_iterator_state_t* data, ss_plugin_table_entry_t* entry) {
  auto* state = reinterpret_cast<cgroup_iteration_state*>(data);
  ss_plugin_state_data value{};
  if (state->reader->read_entry_field(state->cgroup_table, entry, state->plugin->cgroup_path, &value) != SS_PLUGIN_SUCCESS) {
    return 0;
  }
  if (value.str != nullptr) {
    state->container_id = ContainerIDFromCgroup(value.str);
  }
  return 1;
}

ss_plugin_rc CacheContainerID(plugin_state* state,
                              ss_plugin_table_entry_t* thread,
                              ss_plugin_table_reader_vtable_ext* reader,
                              ss_plugin_table_writer_vtable_ext* writer) {
  ss_plugin_state_data cgroups{};
  if (reader->read_entry_field(state->threads, thread, state->cgroups, &cgroups) != SS_PLUGIN_SUCCESS ||
      cgroups.table == nullptr) {
    state->last_error = "failed to read thread cgroups";
    return SS_PLUGIN_FAILURE;
  }

  cgroup_iteration_state iteration{state, reader, cgroups.table, {}};
  if (!reader->iterate_entries(cgroups.table, FindContainerID,
                               reinterpret_cast<ss_plugin_table_iterator_state_t*>(&iteration))) {
    state->last_error = "failed to inspect thread cgroups";
    return SS_PLUGIN_FAILURE;
  }

  ss_plugin_state_data value{};
  const std::string id = iteration.container_id.empty() ? std::string(kHostContainerID) : iteration.container_id;
  value.str = id.c_str();
  if (writer->write_entry_field(state->threads, thread, state->container_id, &value) != SS_PLUGIN_SUCCESS) {
    state->last_error = "failed to cache thread container ID";
    return SS_PLUGIN_FAILURE;
  }
  return SS_PLUGIN_SUCCESS;
}

struct thread_iteration_state {
  plugin_state* plugin;
  ss_plugin_table_reader_vtable_ext* reader;
  ss_plugin_table_writer_vtable_ext* writer;
};

ss_plugin_bool CacheInitialThread(ss_plugin_table_iterator_state_t* data, ss_plugin_table_entry_t* entry) {
  auto* state = reinterpret_cast<thread_iteration_state*>(data);
  return CacheContainerID(state->plugin, entry, state->reader, state->writer) == SS_PLUGIN_SUCCESS;
}

}  // namespace

extern "C" const char* plugin_get_required_api_version() {
  return PLUGIN_API_VERSION_STR;
}

extern "C" const char* plugin_get_version() {
  return kPluginVersion.data();
}

extern "C" const char* plugin_get_name() {
  return kPluginName.data();
}

extern "C" const char* plugin_get_description() {
  return "Caches Collector container IDs in Falco thread state";
}

extern "C" const char* plugin_get_contact() {
  return "https://github.com/stackrox/collector";
}

extern "C" const char* plugin_get_required_event_schema_version(ss_plugin_t*) {
  return "4.1.0";
}

extern "C" ss_plugin_t* plugin_init(const ss_plugin_init_input* input, ss_plugin_rc* rc) {
  auto* state = new plugin_state;
  *rc = SS_PLUGIN_FAILURE;
  if (input == nullptr || input->tables == nullptr || input->tables->fields_ext == nullptr ||
      input->tables->reader_ext == nullptr || input->tables->writer_ext == nullptr) {
    state->last_error = "Falco table API is unavailable";
    return reinterpret_cast<ss_plugin_t*>(state);
  }

  state->threads = input->tables->get_table(input->owner, "threads", SS_PLUGIN_ST_INT64);
  if (state->threads == nullptr) {
    state->last_error = "failed to access Falco threads table";
    return reinterpret_cast<ss_plugin_t*>(state);
  }
  state->cgroups = input->tables->fields_ext->get_table_field(state->threads, "cgroups", SS_PLUGIN_ST_TABLE);
  state->container_id = input->tables->fields_ext->add_table_field(state->threads, "container_id", SS_PLUGIN_ST_STRING);
  if (state->cgroups == nullptr || state->container_id == nullptr) {
    state->last_error = "failed to access Falco thread cgroup or container ID fields";
    return reinterpret_cast<ss_plugin_t*>(state);
  }

  ss_plugin_table_entry_t* entry = input->tables->writer_ext->create_table_entry(state->threads);
  ss_plugin_state_data cgroups{};
  if (entry == nullptr || input->tables->reader_ext->read_entry_field(state->threads, entry, state->cgroups, &cgroups) != SS_PLUGIN_SUCCESS ||
      cgroups.table == nullptr) {
    if (entry != nullptr) {
      input->tables->writer_ext->destroy_table_entry(state->threads, entry);
    }
    state->last_error = "failed to access Falco cgroup table";
    return reinterpret_cast<ss_plugin_t*>(state);
  }
  state->cgroup_path = input->tables->fields_ext->get_table_field(cgroups.table, "second", SS_PLUGIN_ST_STRING);
  input->tables->writer_ext->destroy_table_entry(state->threads, entry);
  if (state->cgroup_path == nullptr) {
    state->last_error = "failed to access Falco cgroup path field";
    return reinterpret_cast<ss_plugin_t*>(state);
  }

  *rc = SS_PLUGIN_SUCCESS;
  return reinterpret_cast<ss_plugin_t*>(state);
}

extern "C" void plugin_destroy(ss_plugin_t* plugin) {
  delete reinterpret_cast<plugin_state*>(plugin);
}

extern "C" const char* plugin_get_last_error(ss_plugin_t* plugin) {
  return reinterpret_cast<plugin_state*>(plugin)->last_error.c_str();
}

extern "C" const char* plugin_get_parse_event_sources() {
  return "[\"syscall\"]";
}

extern "C" uint16_t* plugin_get_parse_event_types(uint32_t* count, ss_plugin_t*) {
  static uint16_t event_types[] = {
      PPME_SYSCALL_CLONE_20_X,
      PPME_SYSCALL_FORK_20_X,
      PPME_SYSCALL_VFORK_20_X,
      PPME_SYSCALL_CLONE3_X,
      PPME_SYSCALL_EXECVE_16_X,
      PPME_SYSCALL_EXECVE_17_X,
      PPME_SYSCALL_EXECVE_18_X,
      PPME_SYSCALL_EXECVE_19_X,
      PPME_SYSCALL_EXECVEAT_X,
      PPME_SYSCALL_CHROOT_X,
  };
  *count = sizeof(event_types) / sizeof(event_types[0]);
  return event_types;
}

extern "C" const char* plugin_get_fields() {
  return R"([{"type":"string","name":"container.id","desc":"Cached container ID for the event thread"}])";
}

extern "C" const char* plugin_get_extract_event_sources() {
  return "[\"syscall\"]";
}

extern "C" ss_plugin_rc plugin_extract_fields(ss_plugin_t* plugin,
                                              const ss_plugin_event_input* event,
                                              const ss_plugin_field_extract_input* input) {
  auto* state = reinterpret_cast<plugin_state*>(plugin);
  ss_plugin_state_data key{};
  key.s64 = static_cast<int64_t>(event->evt->tid);
  ss_plugin_table_entry_t* thread = input->table_reader_ext->get_table_entry(state->threads, &key);
  if (thread == nullptr) {
    for (uint32_t i = 0; i < input->num_fields; ++i) {
      input->fields[i].res_len = 0;
    }
    return SS_PLUGIN_SUCCESS;
  }

  ss_plugin_state_data value{};
  const ss_plugin_rc read_rc = input->table_reader_ext->read_entry_field(state->threads, thread, state->container_id, &value);
  input->table_reader_ext->release_table_entry(state->threads, thread);
  if (read_rc != SS_PLUGIN_SUCCESS || value.str == nullptr) {
    state->last_error = "failed to read cached thread container ID";
    return SS_PLUGIN_FAILURE;
  }

  state->extracted_container_id = value.str;
  state->extracted_container_id_ptr = state->extracted_container_id.c_str();
  for (uint32_t i = 0; i < input->num_fields; ++i) {
    if (input->fields[i].field_id != 0) {
      input->fields[i].res_len = 0;
      continue;
    }
    input->fields[i].res.str = &state->extracted_container_id_ptr;
    input->fields[i].res_len = 1;
  }
  return SS_PLUGIN_SUCCESS;
}

extern "C" ss_plugin_rc plugin_parse_event(ss_plugin_t* plugin,
                                           const ss_plugin_event_input* event,
                                           const ss_plugin_event_parse_input* input) {
  auto* state = reinterpret_cast<plugin_state*>(plugin);
  ss_plugin_state_data key{};
  key.s64 = static_cast<int64_t>(event->evt->tid);
  ss_plugin_table_entry_t* thread = input->table_reader_ext->get_table_entry(state->threads, &key);
  if (thread == nullptr) {
    return SS_PLUGIN_SUCCESS;
  }
  const ss_plugin_rc rc = CacheContainerID(state, thread, input->table_reader_ext, input->table_writer_ext);
  input->table_reader_ext->release_table_entry(state->threads, thread);
  return rc;
}

extern "C" ss_plugin_rc plugin_capture_open(ss_plugin_t* plugin, const ss_plugin_capture_listen_input* input) {
  auto* state = reinterpret_cast<plugin_state*>(plugin);
  thread_iteration_state iteration{state, input->table_reader_ext, input->table_writer_ext};
  if (!input->table_reader_ext->iterate_entries(
          state->threads, CacheInitialThread,
          reinterpret_cast<ss_plugin_table_iterator_state_t*>(&iteration))) {
    if (state->last_error.empty()) {
      state->last_error = "failed to cache initial thread container IDs";
    }
    return SS_PLUGIN_FAILURE;
  }
  return SS_PLUGIN_SUCCESS;
}

extern "C" ss_plugin_rc plugin_capture_close(ss_plugin_t*, const ss_plugin_capture_listen_input*) {
  return SS_PLUGIN_SUCCESS;
}
