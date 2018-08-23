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

#include "EventClassifier.h"

extern "C" {
#include <string.h>
}

#include <functional>
#include <sstream>

#include "EventNames.h"

namespace collector {

namespace {

constexpr uint64_t kNetFdTypes =
    (1 << SCAP_FD_IPV4_SOCK)
    | (1 << SCAP_FD_IPV4_SERVSOCK)
    | (1 << SCAP_FD_IPV6_SOCK)
    | (1 << SCAP_FD_IPV6_SERVSOCK)
    | (1 << SCAP_FD_UNIX_SOCK);

}  // namespace

void EventClassifier::Init(const std::string& hostname, const std::vector<std::string>& process_syscalls,
    const std::vector<std::string>& generic_syscalls) {
  hostname_hash_ = std::hash<std::string>()(hostname);
  process_syscalls_.reset();
  generic_syscalls_.reset();
  const EventNames& event_names = EventNames::GetInstance();
  for (const std::string& syscall_name : process_syscalls) {
    for (ppm_event_type event_type : event_names.GetEventIDs(syscall_name)) {
      process_syscalls_.set(event_type);
    }
  }
  for (const std::string& syscall_name : generic_syscalls) {
    for (ppm_event_type event_type : event_names.GetEventIDs(syscall_name)) {
      generic_syscalls_.set(event_type);
    }
  }
}

SignalType EventClassifier::Classify(SafeBuffer* key_buf, sinsp_evt* event) const {
  key_buf->clear();
  if (generic_syscalls_.test(event->get_type())) {
    if (!ExtractProcessSignalKey(key_buf, event)) return SIGNAL_TYPE_UNKNOWN;
    return SIGNAL_TYPE_GENERIC;
  }
  if (process_syscalls_.test(event->get_type())) {
    if (!ExtractProcessSignalKey(key_buf, event)) return SIGNAL_TYPE_UNKNOWN;
    return SIGNAL_TYPE_PROCESS;
  }
  sinsp_evt::category cat;
  event->get_category(&cat);
  if (cat.m_category == EC_NET || ((cat.m_category & EC_IO_BASE) && cat.m_subcategory == sinsp_evt::SC_NET)) {
    if (!ExtractNetworkSignalKey(key_buf, event)) return SIGNAL_TYPE_UNKNOWN;
    return SIGNAL_TYPE_NETWORK;
  }
  sinsp_fdinfo_t* fd_info = event->get_fd_info();
  if (fd_info && ((1 << fd_info->m_type) & kNetFdTypes)) {
    if (!ExtractNetworkSignalKey(key_buf, event)) return SIGNAL_TYPE_UNKNOWN;
    return SIGNAL_TYPE_NETWORK;
  }
  if (!ExtractFileSignalKey(key_buf, event)) return SIGNAL_TYPE_UNKNOWN;
  return SIGNAL_TYPE_FILE;
}

bool EventClassifier::ExtractNetworkSignalKey(SafeBuffer* key_buf, sinsp_evt* event) const {
  sinsp_fdinfo_t* fd_info = event->get_fd_info();
  if (!fd_info) return false;
  if (fd_info->m_type == SCAP_FD_IPV4_SOCK) {
    const auto& ipv4_fields = fd_info->m_sockinfo.m_ipv4info.m_fields;
    if (!ipv4_fields.m_sip) return false;
    key_buf->AppendFTrunc("%" PRIx32 "%" PRIx16, ipv4_fields.m_sip, ipv4_fields.m_sport);
    return true;
  }

  if (fd_info->m_type == SCAP_FD_IPV6_SOCK) {
    const auto& ipv6_fields = fd_info->m_sockinfo.m_ipv6info.m_fields;
    if (!ipv6_fields.m_sip[0] && !ipv6_fields.m_sip[1] && !ipv6_fields.m_sip[2] && !ipv6_fields.m_sip[3]) return false;
    key_buf->AppendFTrunc(
        "%" PRIx32 "%" PRIx32 "%" PRIx32 "%" PRIx32 "%" PRIx16,
        ipv6_fields.m_sip[0], ipv6_fields.m_sip[1], ipv6_fields.m_sip[2], ipv6_fields.m_sip[3], ipv6_fields.m_sport);
    return true;
  }

  if (fd_info->m_type == SCAP_FD_UNIX_SOCK) {
    const auto& unix_fields = fd_info->m_sockinfo.m_unixinfo.m_fields;
    key_buf->AppendFTrunc(
        "%" PRIx64 "%" PRIx32 "%" PRIx32,
        hostname_hash_, static_cast<int32_t>(unix_fields.m_source >> 3), static_cast<int32_t>(unix_fields.m_dest >> 3));
    return true;
  }

  return false;
}

bool EventClassifier::ExtractProcessSignalKey(SafeBuffer* key_buf, sinsp_evt* event) {
  const sinsp_threadinfo* tinfo = event->get_thread_info();
  if (!tinfo) return false;
  key_buf->AppendTrunc(tinfo->m_container_id);
  return true;
}

bool EventClassifier::ExtractFileSignalKey(SafeBuffer* key_buf, sinsp_evt* event) {
  const sinsp_threadinfo* tinfo = event->get_thread_info();
  if (!tinfo) return false;
  // Use "<pid>,<fd>,<container-id>" as the key. We place the container ID last to make sure the pid and fd don't get
  // truncated.
  key_buf->AppendFTrunc("%" PRId64 ",%" PRId64 ",%s", tinfo->m_pid, event->get_fd_num(), tinfo->m_container_id.c_str());
  return true;
}

}  // namespace collector
