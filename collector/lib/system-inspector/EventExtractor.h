#pragma once

#include <string>
#include <vector>

#include "libsinsp/filterchecks.h"
#include "libsinsp/sinsp.h"

#include "Logging.h"

namespace collector::system_inspector {

// This class allows extracting a predefined set of system_inspector event fields in an efficient manner.
class EventExtractor {
 public:
  void Init(sinsp* inspector);
  void ClearWrappers();

  static sinsp_filter_check_list& FilterList() {
    static sinsp_filter_check_list filterlist;
    return filterlist;
  }

 private:
  struct FilterCheckWrapper {
    FilterCheckWrapper(EventExtractor* extractor, const char* event_name) : event_name(event_name) {
      extractor->wrappers_.push_back(this);
    }

    sinsp_filter_check* operator->() { return filter_check.get(); }

    const char* event_name;
    std::unique_ptr<sinsp_filter_check> filter_check;
  };

  std::vector<FilterCheckWrapper*> wrappers_;

#define DECLARE_FILTER_CHECK(id, fieldname) \
  FilterCheckWrapper filter_check_##id##_ = {this, fieldname}

#define FIELD_RAW(id, fieldname, type)                                                                     \
 public:                                                                                                   \
  const type* get_##id(sinsp_evt* event) {                                                                 \
    if (!filter_check_##id##_.filter_check) return nullptr;                                                \
    std::vector<extract_value_t> vals_##id;                                                                \
    if (!filter_check_##id##_->extract(event, vals_##id)) return nullptr;                                  \
    if (vals_##id.empty()) return nullptr;                                                                 \
    auto len = vals_##id[0].len;                                                                           \
    auto buf = vals_##id[0].ptr;                                                                           \
    if (len != sizeof(type)) {                                                                             \
      CLOG_THROTTLED(WARNING, std::chrono::seconds(30))                                                    \
          << "Failed to extract value for field " << fieldname << ": expected type " << #type << " (size " \
          << sizeof(type) << "), but returned value has size " << len;                                     \
      return nullptr;                                                                                      \
    }                                                                                                      \
    return reinterpret_cast<const type*>(buf);                                                             \
  }                                                                                                        \
                                                                                                           \
 private:                                                                                                  \
  DECLARE_FILTER_CHECK(id, fieldname)

// When using FIELD_RAW_SAFE, type needs to be trivially copyable so we
// can move it to a properly aligned variable.
#define FIELD_RAW_SAFE(id, fieldname, type)                                                                \
 public:                                                                                                   \
  const std::optional<type> get_##id(sinsp_evt* event) {                                                   \
    static_assert(std::is_trivially_copyable_v<type>,                                                      \
                  "Attempted to create FIELD_RAW_SAFE on non trivial type");                               \
    if (!filter_check_##id##_.filter_check) return {};                                                     \
    std::vector<extract_value_t> vals_##id;                                                                \
    if (!filter_check_##id##_->extract(event, vals_##id)) return {};                                       \
    if (vals_##id.empty()) return {};                                                                      \
    auto len = vals_##id[0].len;                                                                           \
    auto buf = vals_##id[0].ptr;                                                                           \
    if (len != sizeof(type)) {                                                                             \
      CLOG_THROTTLED(WARNING, std::chrono::seconds(30))                                                    \
          << "Failed to extract value for field " << fieldname << ": expected type " << #type << " (size " \
          << sizeof(type) << "), but returned value has size " << len;                                     \
      return {};                                                                                           \
    }                                                                                                      \
    type val;                                                                                              \
    std::memcpy(&val, buf, sizeof(type));                                                                  \
    return {val};                                                                                          \
  }                                                                                                        \
                                                                                                           \
 private:                                                                                                  \
  DECLARE_FILTER_CHECK(id, fieldname)

#define FIELD_CSTR(id, fieldname)                                         \
 public:                                                                  \
  const char* get_##id(sinsp_evt* event) {                                \
    if (!filter_check_##id##_.filter_check) return nullptr;               \
    std::vector<extract_value_t> vals_##id;                               \
    if (!filter_check_##id##_->extract(event, vals_##id)) return nullptr; \
    if (vals_##id.empty()) return nullptr;                                \
    return reinterpret_cast<const char*>(vals_##id[0].ptr);               \
  }                                                                       \
                                                                          \
 private:                                                                 \
  DECLARE_FILTER_CHECK(id, fieldname)

#define EVT_ARG(name) FIELD_CSTR(evt_arg_##name, "evt.arg." #name)
#define EVT_ARG_RAW(name, type) FIELD_RAW(evt_arg_##name, "evt.rawarg." #name, type)

#define TINFO_FIELD_RAW(id, fieldname, type)            \
 public:                                                \
  const type* get_##id(sinsp_evt* event) {              \
    if (!event) return nullptr;                         \
    sinsp_threadinfo* tinfo = event->get_thread_info(); \
    if (!tinfo) return nullptr;                         \
    return &tinfo->fieldname;                           \
  }

#define TINFO_FIELD_RAW_GETTER(id, getter, type)        \
 public:                                                \
  type internal_##id;                                   \
  const type* get_##id(sinsp_evt* event) {              \
    if (!event) return nullptr;                         \
    sinsp_threadinfo* tinfo = event->get_thread_info(); \
    if (!tinfo) return nullptr;                         \
    internal_##id = tinfo->getter();                    \
    return &internal_##id;                              \
  }

#define TINFO_FIELD(id) TINFO_FIELD_RAW(id, m_##id, decltype(std::declval<sinsp_threadinfo>().m_##id))

  // Fields can be made available for querying by using a number of macros:
  // - TINFO_FIELD_RAW(id, fieldname, type): exposes the m_<fieldname> field of threadinfo via get_<id>()
  // - TINFO_FIELD(name): exposes the m_<name> field of threadinfo via get_<name>()
  // - FIELD_CSTR(id, fieldname): exposes the system inspector field <fieldname> via get_<id>(), returning a null-terminated
  //   const char*.
  // - FIELD_RAW(id, fieldname, type): exposes the system inspector field <fieldname> via get_<id>(), returning a const <type>*.
  // - FIELD_RAW_SAFE(id, fieldname, type): exposes the system inspector field <fieldname> via get_<id>(), returning a std::optional<type>.
  // - EVT_ARG(argname): shorthand for FIELD_CSTR(evt_arg_<argname>, "evt.arg.<argname>")
  // - EVT_ARG_RAW(argname, type): shorthand for FIELD_RAW(evt_arg_<argname>, "evt.rawarg.<argname>", <type>)
  //
  // ADD ANY NEW FIELDS BELOW THIS LINE

  // Container related fields — provided by the container plugin via filter fields.
  FIELD_CSTR(container_id, "container.id");

  // Process related fields
  TINFO_FIELD(comm);
  TINFO_FIELD(exe);
  TINFO_FIELD(exepath);
  TINFO_FIELD(pid);
  TINFO_FIELD_RAW(uid, m_uid, uint32_t);
  TINFO_FIELD_RAW(gid, m_gid, uint32_t);
  FIELD_CSTR(proc_args, "proc.args");

  // General event information
  FIELD_RAW_SAFE(event_rawres, "evt.rawres", int64_t);

  // File/network related
  FIELD_RAW_SAFE(client_port, "fd.cport", uint16_t);
  FIELD_RAW_SAFE(server_port, "fd.sport", uint16_t);

  // k8s metadata
  FIELD_CSTR(k8s_namespace, "k8s.ns.name");

#undef TINFO_FIELD
#undef FIELD_RAW
#undef FIELD_CSTR
#undef EVT_ARG
#undef EVT_ARG_RAW
#undef DECLARE_FILTER_CHECK
};

}  // namespace collector::system_inspector
