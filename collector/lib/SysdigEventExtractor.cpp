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
#include "SysdigEventExtractor.h"

namespace collector {

void SysdigEventExtractor::Init(sinsp* inspector) {
  filter_check_container_id_.reset(sinsp_filter_check_iface::get("container.id", inspector));
  filter_check_container_privileged_.reset(sinsp_filter_check_iface::get("container.privileged", inspector));
  filter_check_evt_type_.reset(sinsp_filter_check_iface::get("evt.type", inspector));
  filter_check_evt_time_.reset(sinsp_filter_check_iface::get("evt.time", inspector));
  filter_check_evt_rawtime_.reset(sinsp_filter_check_iface::get("evt.rawtime", inspector));
  filter_check_evt_dir_.reset(sinsp_filter_check_iface::get("evt.dir", inspector));
  filter_check_evt_buflen_.reset(sinsp_filter_check_iface::get("evt.buflen", inspector));
  filter_check_evt_io_dir_.reset(sinsp_filter_check_iface::get("evt.io_dir", inspector));
  filter_check_evt_res_.reset(sinsp_filter_check_iface::get("evt.res", inspector));
  filter_check_fd_cip_.reset(sinsp_filter_check_iface::get("fd.cip", inspector));
  filter_check_fd_cport_.reset(sinsp_filter_check_iface::get("fd.cport", inspector));
  filter_check_fd_l4proto_.reset(sinsp_filter_check_iface::get("fd.l4proto", inspector));
  filter_check_fd_lproto_.reset(sinsp_filter_check_iface::get("fd.lproto", inspector));
  filter_check_fd_name_.reset(sinsp_filter_check_iface::get("fd.name", inspector));
  filter_check_fd_rip_.reset(sinsp_filter_check_iface::get("fd.rip", inspector));
  filter_check_fd_rproto_.reset(sinsp_filter_check_iface::get("fd.rproto", inspector));
  filter_check_fd_sip_.reset(sinsp_filter_check_iface::get("fd.sip", inspector));
  filter_check_fd_sockfamily_.reset(sinsp_filter_check_iface::get("fd.sockfamily", inspector));
  filter_check_fd_sport_.reset(sinsp_filter_check_iface::get("fd.sport", inspector));
  filter_check_fd_type_.reset(sinsp_filter_check_iface::get("fd.type", inspector));
  filter_check_group_gid_.reset(sinsp_filter_check_iface::get("group.gid", inspector));
  filter_check_proc_cmdline_.reset(sinsp_filter_check_iface::get("proc.cmdline", inspector));
  filter_check_proc_cwd_.reset(sinsp_filter_check_iface::get("proc.cwd", inspector));
  filter_check_proc_exe_.reset(sinsp_filter_check_iface::get("proc.exe", inspector));
  filter_check_proc_exeline_.reset(sinsp_filter_check_iface::get("proc.exeline", inspector));
  filter_check_proc_name_.reset(sinsp_filter_check_iface::get("proc.name", inspector));
  filter_check_proc_pname_.reset(sinsp_filter_check_iface::get("proc.pname", inspector));
  filter_check_user_name_.reset(sinsp_filter_check_iface::get("user.name", inspector));
  filter_check_user_shell_.reset(sinsp_filter_check_iface::get("user.shell", inspector));
  filter_check_user_uid_.reset(sinsp_filter_check_iface::get("user.uid", inspector));
  filter_check_evt_args_.reset(sinsp_filter_check_iface::get("evt.args", inspector));
  filter_check_proc_pid_.reset(sinsp_filter_check_iface::get("proc.pid", inspector));
  filter_check_fd_num_.reset(sinsp_filter_check_iface::get("fd.num", inspector));
  filter_check_proc_aname_.reset(sinsp_filter_check_iface::get("proc.aname", inspector));
}

/*
 * Process related methods
 */
const char* SysdigEventExtractor::event_type(sinsp_evt* evt) {
  return filter_check_evt_type_->tostring(evt);
}

const char* SysdigEventExtractor::event_args(sinsp_evt* evt) {
  return filter_check_evt_args_->tostring(evt);
}

const char* SysdigEventExtractor::event_res(sinsp_evt* evt) {
  return filter_check_evt_res_->tostring(evt);
}

const int64_t* SysdigEventExtractor::get_pid(sinsp_evt* evt) {
  if (!evt) return nullptr;
  const sinsp_threadinfo* tinfo = evt->get_thread_info();
  if (!tinfo) return nullptr;
  return &tinfo->m_pid;
}

const int64_t* SysdigEventExtractor::get_tid(sinsp_evt* evt) {
  if (!evt) return nullptr;
  const sinsp_threadinfo* tinfo = evt->get_thread_info();
  if (!tinfo) return nullptr;
  return &tinfo->m_tid;
}

const std::string* SysdigEventExtractor::get_comm(sinsp_evt* evt) {
  if (!evt) return nullptr;
  const sinsp_threadinfo* tinfo = evt->get_thread_info();
  if (!tinfo) return nullptr;
  return &tinfo->m_comm;
}

const std::string* SysdigEventExtractor::get_exe(sinsp_evt* evt) {
  if (!evt) return nullptr;
  const sinsp_threadinfo* tinfo = evt->get_thread_info();
  if (!tinfo) return nullptr;
  return &tinfo->m_exe;
}

const std::string* SysdigEventExtractor::get_exepath(sinsp_evt* evt) {
  if (!evt) return nullptr;
  const sinsp_threadinfo* tinfo = evt->get_thread_info();
  if (!tinfo) return nullptr;
  return &tinfo->m_exepath;
}

const char* SysdigEventExtractor::get_cwd(sinsp_evt* evt) {
  return filter_check_proc_cwd_->tostring(evt);
}

/*
 * Container related methods
 */
bool SysdigEventExtractor::container_privileged(sinsp_evt* evt) {
  uint32_t len;
  auto raw_buf = filter_check_container_privileged_->extract(evt, &len);
  if (len != sizeof(uint32_t)) {
    return false;
  }
  return *reinterpret_cast<uint32_t*>(raw_buf) != 0;
}

const std::string* SysdigEventExtractor::container_id(sinsp_evt* evt) {
  if (!evt) return nullptr;
  const sinsp_threadinfo* tinfo = evt->get_thread_info();
  if (!tinfo) return nullptr;
  return &tinfo->m_container_id;
}

}  // namespace collector
