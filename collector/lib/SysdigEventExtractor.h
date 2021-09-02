/** collector

A full notice with attributions is provided along with this source code.

This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License version 2 as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program; if not, write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

* In addition, as a special exception, the copyright holders give
* permission to link the code of portions of this program with the
* OpenSSL library under certain conditions as described in each
* individual source file, and distribute linked combinations
* including the two.
* You must obey the GNU General Public License in all respects
* for all of the code used other than OpenSSL.  If you modify
* file(s) with this exception, you may extend this exception to your
* version of the file(s), but you are not obligated to do so.  If you
* do not wish to do so, delete this exception statement from your
* version.
*/

#ifndef _SYSDIG_EVENT_EXTRACTOR_H_
#define _SYSDIG_EVENT_EXTRACTOR_H_

#include <string>
#include <vector>

#include "libsinsp/sinsp.h"

#include "Logging.h"

namespace collector {

// This class allows extracting a predefined set of Sysdig event fields in an efficient manner.
class SysdigEventExtractor {
 public:
  void Init(sinsp* inspector);

 private:
  struct FilterCheckWrapper {
    FilterCheckWrapper(SysdigEventExtractor* extractor, const char* event_name) : event_name(event_name) {
      extractor->wrappers_.push_back(this);
    }

    sinsp_filter_check_iface* operator->() const { return filter_check.get(); }

    const char* event_name;
    std::unique_ptr<sinsp_filter_check_iface> filter_check;
  };

  std::vector<FilterCheckWrapper*> wrappers_;

#define DECLARE_FILTER_CHECK(id, fieldname) \
  FilterCheckWrapper filter_check_##id##_ = {this, fieldname}

#define FIELD_RAW(id, fieldname, type)                                                                     \
 public:                                                                                                   \
  const type* get_##id(sinsp_evt* event) {                                                                 \
    uint32_t len;                                                                                          \
    auto buf = filter_check_##id##_->extract(event, &len);                                                 \
    if (!buf) return nullptr;                                                                              \
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

#define FIELD_CSTR(id, fieldname)                          \
 public:                                                   \
  const char* get_##id(sinsp_evt* event) {                 \
    uint32_t len;                                          \
    auto buf = filter_check_##id##_->extract(event, &len); \
    if (!buf) return nullptr;                              \
    return reinterpret_cast<const char*>(buf);             \
  }                                                        \
                                                           \
 private:                                                  \
  DECLARE_FILTER_CHECK(id, fieldname)

#define EVT_ARG(name) FIELD_CSTR(evt_arg_##name, "evt.arg." #name)
#define EVT_ARG_RAW(name, type) FIELD_RAW(evt_arg_##name, "evt.rawarg." #name, type)

#define TINFO_FIELD(id)                                                                 \
 public:                                                                                \
  const decltype(std::declval<sinsp_threadinfo>().m_##id)* get_##id(sinsp_evt* event) { \
    if (!event) return nullptr;                                                         \
    sinsp_threadinfo* tinfo = event->get_thread_info(true);                             \
    if (!tinfo) return nullptr;                                                         \
    return &tinfo->m_##id;                                                              \
  }

  // Fields can be made available for querying by using a number of macros:
  // - TINFO_FIELD(name): exposes the m_<name> field of threadinfo via get_<name>()
  // - FIELD_CSTR(id, fieldname): exposes the sysdig field <fieldname> via get_<id>(), returning a null-terminated
  //   const char*.
  // - FIELD_RAW(id, fieldname, type): exposes the sysdig field <fieldname> via get_<id>(), returning a const <type>*.
  // - EVT_ARG(argname): shorthand for FIELD_CSTR(evt_arg_<argname>, "evt.arg.<argname>")
  // - EVT_ARG_RAW(argname, type): shorthand for FIELD_RAW(evt_arg_<argname>, "evt.rawarg.<argname>", <type>)
  //
  // ADD ANY NEW FIELDS BELOW THIS LINE

  // Container related fields
  TINFO_FIELD(container_id);
  FIELD_RAW(container_privileged, "container.privileged", uint32_t);

  // Process related fields
  TINFO_FIELD(comm);
  TINFO_FIELD(exe);
  TINFO_FIELD(exepath);
  TINFO_FIELD(pid);
  TINFO_FIELD(tid);
  TINFO_FIELD(uid);
  TINFO_FIELD(gid);
  FIELD_CSTR(proc_name, "proc.name");
  FIELD_CSTR(proc_pname, "proc.pname");
  FIELD_CSTR(proc_args, "proc.args");
  FIELD_CSTR(exeline, "proc.exeline");
  FIELD_CSTR(cmdline, "proc.cmdline");
  FIELD_CSTR(user_name, "user.name");
  FIELD_CSTR(cwd, "proc.cwd");
  FIELD_CSTR(evt_args, "evt.args");
  FIELD_RAW(ppid, "proc.ppid", int64_t);

  // General event information
  FIELD_RAW(event_rawres, "evt.rawres", int64_t);
  EVT_ARG(name);
  EVT_ARG(newpath);
  EVT_ARG(oldpath);
  EVT_ARG(path);
  EVT_ARG(target);
  EVT_ARG(linkpath);
  EVT_ARG_RAW(fd, int64_t);
  EVT_ARG_RAW(flags, int32_t);
  EVT_ARG_RAW(olddir, int64_t);
  EVT_ARG_RAW(newdir, int64_t);
  EVT_ARG_RAW(olddirfd, int64_t);
  EVT_ARG_RAW(newdirfd, int64_t);
  EVT_ARG_RAW(linkdirfd, int64_t);
  EVT_ARG_RAW(dirfd, int64_t);

  // File/network related
  FIELD_RAW(client_port, "fd.cport", uint16_t);
  FIELD_RAW(server_port, "fd.sport", uint16_t);
  FIELD_CSTR(fd_name, "fd.name");

#undef TINFO_FIELD
#undef FIELD_RAW
#undef FIELD_CSTR
#undef EVT_ARG
#undef EVT_ARG_RAW
#undef DECLARE_FILTER_CHECK
};

}  // namespace collector

#endif  // _SYSDIG_EVENT_EXTRACTOR_H_
