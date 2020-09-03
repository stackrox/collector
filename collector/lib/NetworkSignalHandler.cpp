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
#include "sinsp_errno.h"

#include "NetworkSignalHandler.h"

#include "EventMap.h"

namespace collector {

namespace {

enum class Modifier : uint8_t {
  INVALID = 0,
  ADD,
  REMOVE,
};

EventMap<Modifier> modifiers = {
    {
        {"shutdown<", Modifier::REMOVE},
        {"connect<", Modifier::ADD},
        {"accept<", Modifier::ADD},
    },
    Modifier::INVALID,
};

}  // namespace

std::pair<Connection, bool> NetworkSignalHandler::GetConnection(sinsp_evt* evt) {
  const int64_t *res = event_extractor_.get_event_rawres(evt);
  if (!res || *res < 0) {
    // track non-blocking connect attempts (EINPROGRESS)
    if (!res || *res != -SE_EINPROGRESS) {
      // ignore unsuccessful events for now.
      return {{}, false};
    }
  }

  auto* fd_info = evt->get_fd_info();
  if (!fd_info) return {{}, false};

  bool is_server = fd_info->is_role_server();
  if (!is_server && !fd_info->is_role_client()) {
    return {{}, false};
  }

  L4Proto l4proto;
  switch (fd_info->get_l4proto()) {
    case SCAP_L4_TCP:
      l4proto = L4Proto::TCP;
      break;
    case SCAP_L4_UDP:
      l4proto = L4Proto::UDP;
      break;
    default:
      return {{}, false};
  }

  Endpoint client, server;
  switch (fd_info->m_type) {
    case SCAP_FD_IPV4_SOCK: {
      const auto& ipv4_fields = fd_info->m_sockinfo.m_ipv4info.m_fields;
      client = Endpoint(Address(ipv4_fields.m_sip), ipv4_fields.m_sport);
      server = Endpoint(Address(ipv4_fields.m_dip), ipv4_fields.m_dport);
      break;
    }
    case SCAP_FD_IPV6_SOCK: {
      const auto& ipv6_fields = fd_info->m_sockinfo.m_ipv6info.m_fields;
      client = Endpoint(Address(ipv6_fields.m_sip.m_b), ipv6_fields.m_sport);
      server = Endpoint(Address(ipv6_fields.m_dip.m_b), ipv6_fields.m_dport);
      break;
    }
    default:
      return {{}, false};
  }

  const Endpoint* local = is_server ? &server : &client;
  const Endpoint* remote = is_server ? &client : &server;

  const std::string* container_id = event_extractor_.get_container_id(evt);
  if (!container_id) return {{}, false};
  return {Connection(*container_id, *local, *remote, l4proto, is_server), true};
}

SignalHandler::Result NetworkSignalHandler::HandleSignal(sinsp_evt* evt) {
  auto modifier = modifiers[evt->get_type()];
  if (modifier == Modifier::INVALID) return SignalHandler::IGNORED;

  auto result = GetConnection(evt);
  if (!result.second || !IsRelevantConnection(result.first)) {
    return SignalHandler::IGNORED;
  }

  conn_tracker_->UpdateConnection(result.first, evt->get_ts() / 1000UL, modifier == Modifier::ADD);
  return SignalHandler::PROCESSED;
}

std::vector<string> NetworkSignalHandler::GetRelevantEvents() {
  return {"shutdown<", "connect<", "accept<"};
}

}  // namespace collector
