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

#include "NetworkSignalFormatter.h"

#include <arpa/inet.h>

#include "EventMap.h"
#include "Logging.h"

namespace collector {

using Signal = NetworkSignalFormatter::Signal;
using KernelSignal = NetworkSignalFormatter::KernelSignal;
using NetworkSignal = NetworkSignalFormatter::NetworkSignal;
using NetworkSignalType = NetworkSignalFormatter::NetworkSignalType;
using NetworkAddress = NetworkSignalFormatter::NetworkAddress;
using ProcessDetails = NetworkSignalFormatter::ProcessDetails;
using ConnectionInfo = NetworkSignalFormatter::ConnectionInfo;
using L4Protocol = data::L4Protocol;
using IPV4NetworkAddress = data::IPV4NetworkAddress;
using IPV6NetworkAddress = data::IPV6NetworkAddress;
using LocalNetworkAddress = data::LocalNetworkAddress;

namespace {

uint64_t make_net_int64(uint32_t net_low, uint32_t net_high) {
  return static_cast<uint64_t>(ntohl(net_high)) << 32 | static_cast<uint64_t>(ntohl(net_low));
}

EventMap<NetworkSignalType> network_signals = {
  {
    { "close<", NetworkSignal::CLOSE },
    { "bind<", NetworkSignal::BIND },
    { "connect<", NetworkSignal::CONNECT },
    { "accept<", NetworkSignal::ACCEPT },
    { "listen<", NetworkSignal::LISTEN },
  },
  NetworkSignal::UNKNOWN,
};

}  // namespace

Signal* NetworkSignalFormatter::ToProtoMessage(sinsp_evt* event) {
  NetworkSignalType signal_type = NetworkSignal::UNKNOWN;

  if (event->get_category() == EC_IO_READ && event->get_direction() == SCAP_ED_OUT) {
    signal_type = NetworkSignal::RECV;
  } else if (event->get_category() == EC_IO_WRITE && event->get_direction() == SCAP_ED_OUT) {
    signal_type = NetworkSignal::SEND;
  } else {
    signal_type = network_signals[event->get_type()];
  }

  NetworkSignal* network_signal = CreateNetworkSignal(event, signal_type);
  if (!network_signal) return nullptr;

  Signal* signal = AllocateRoot();
  signal->set_time_nanos(event->get_ts());

  KernelSignal* kernel_signal = CreateKernelSignal(event);
  kernel_signal->set_allocated_network_signal(network_signal);

  signal->set_allocated_kernel_signal(kernel_signal);

  return signal;
}

NetworkSignal* NetworkSignalFormatter::CreateNetworkSignal(sinsp_evt* event, NetworkSignalType signal_type) {
  if (signal_type == NetworkSignal::UNKNOWN) return nullptr;

  auto signal = Allocate<NetworkSignal>();
  signal->set_type(signal_type);
  if (const int64_t* res = event_extractor_.get_event_rawres(event)) {
    signal->set_result(*res);

    if ((signal_type == NetworkSignal::SEND || signal_type == NetworkSignal::RECV) && *res > 0) {
      signal->set_count_bytes(*res);
    }
  }

  signal->set_allocated_connection(CreateConnectionInfo(event->get_fd_info()));

  return signal;
}

KernelSignal* NetworkSignalFormatter::CreateKernelSignal(sinsp_evt* event) {
  KernelSignal* signal = Allocate<KernelSignal>();
  if (const std::string* container_id = event_extractor_.get_container_id(event)) {
    if (container_id->length() <= 12) {
      signal->set_container_short_id(*container_id);
    } else {
      signal->set_container_short_id(container_id->substr(0, 12));
    }
  }
  if (const int64_t* pid = event_extractor_.get_pid(event)) {
    signal->set_process_pid(*pid);
  }
  if (const uint32_t* privileged = event_extractor_.get_container_privileged(event)) {
    signal->set_container_privileged(*privileged != 0);
  }
  signal->set_allocated_process_details(CreateProcessDetails(event));

  return signal;
}

ProcessDetails* NetworkSignalFormatter::CreateProcessDetails(sinsp_evt* event) {
  const uint32_t* uid = event_extractor_.get_uid(event);
  const uint32_t* gid = event_extractor_.get_gid(event);

  if (!uid && !gid) return nullptr;

  auto details = Allocate<ProcessDetails>();
  if (uid) details->set_uid(*uid);
  if (gid) details->set_gid(*gid);

  return details;
}

ConnectionInfo* NetworkSignalFormatter::CreateConnectionInfo(sinsp_fdinfo_t* fd_info) {
  if (!fd_info) return nullptr;

  auto conn = Allocate<ConnectionInfo>();

  if (!fd_info->is_role_none()) {
    conn->set_role(fd_info->is_role_server() ? ConnectionInfo::ROLE_SERVER : ConnectionInfo::ROLE_CLIENT);
  }

  L4Protocol l4proto = data::L4_PROTOCOL_UNKNOWN;
  switch (fd_info->get_l4proto()) {
    case SCAP_L4_TCP:
      l4proto = data::L4_PROTOCOL_TCP;
      break;
    case SCAP_L4_UDP:
      l4proto = data::L4_PROTOCOL_UDP;
      break;
    case SCAP_L4_ICMP:
      l4proto = data::L4_PROTOCOL_ICMP;
      break;
    case SCAP_L4_RAW:
      l4proto = data::L4_PROTOCOL_RAW;
      break;
    default:
      break;
  }

  if (l4proto != data::L4_PROTOCOL_UNKNOWN) {
    conn->set_protocol(l4proto);
  }

  switch (fd_info->m_type) {
    case SCAP_FD_IPV4_SOCK: {
      const auto& ipv4_fields = fd_info->m_sockinfo.m_ipv4info.m_fields;
      conn->set_socket_family(data::SOCKET_FAMILY_IPV4);
      conn->set_allocated_client_address(CreateIPv4Address(ipv4_fields.m_sip, ipv4_fields.m_sport));
      conn->set_allocated_server_address(CreateIPv4Address(ipv4_fields.m_dip, ipv4_fields.m_dport));
      break;
    }
    case SCAP_FD_IPV4_SERVSOCK: {
      const auto& ipv4_server_info = fd_info->m_sockinfo.m_ipv4serverinfo;
      conn->set_socket_family(data::SOCKET_FAMILY_IPV4);
      conn->set_role(ConnectionInfo::ROLE_SERVER);
      conn->set_allocated_server_address(CreateIPv4Address(ipv4_server_info.m_ip, ipv4_server_info.m_port));
      break;
    }
    case SCAP_FD_IPV6_SOCK: {
      const auto& ipv6_fields = fd_info->m_sockinfo.m_ipv6info.m_fields;
      conn->set_socket_family(data::SOCKET_FAMILY_IPV6);
      conn->set_allocated_client_address(CreateIPv6Address(ipv6_fields.m_sip, ipv6_fields.m_sport));
      conn->set_allocated_server_address(CreateIPv6Address(ipv6_fields.m_dip, ipv6_fields.m_dport));
      break;
    }
    case SCAP_FD_IPV6_SERVSOCK: {
      const auto& ipv6_server_info = fd_info->m_sockinfo.m_ipv6serverinfo;
      conn->set_socket_family(data::SOCKET_FAMILY_IPV6);
      conn->set_role(ConnectionInfo::ROLE_SERVER);
      conn->set_allocated_server_address(CreateIPv6Address(ipv6_server_info.m_ip, ipv6_server_info.m_port));
      break;
    }
    case SCAP_FD_UNIX_SOCK: {
      const auto& unix_fields = fd_info->m_sockinfo.m_unixinfo.m_fields;;
      conn->set_socket_family(data::SOCKET_FAMILY_LOCAL);
      // Extract the socket path (if any) only for the server address.
      conn->set_allocated_client_address(CreateUnixAddress(unix_fields.m_source, nullptr));
      conn->set_allocated_server_address(CreateUnixAddress(unix_fields.m_dest, fd_info));
      break;
    }
    default:
      break;
  }

  return conn;
}


NetworkAddress* NetworkSignalFormatter::CreateIPv4Address(uint32_t ip, uint16_t port) {
  if (!ip) return nullptr;
  auto addr = Allocate<NetworkAddress>();
  IPV4NetworkAddress* ipv4_addr = addr->mutable_ipv4_address();
  ipv4_addr->set_address(ntohl(ip));
  ipv4_addr->set_port(port);
  return addr;
}

NetworkAddress* NetworkSignalFormatter::CreateIPv6Address(const uint32_t (&ip)[4], uint16_t port) {
  if (!ip[0] && !ip[1] && !ip[2] && !ip[3]) return nullptr;
  auto addr = Allocate<NetworkAddress>();
  IPV6NetworkAddress* ipv6_addr = addr->mutable_ipv6_address();
  // Not sure if this is well-defined at all, but I'm assuming that ip is in network order (big endian), hence
  // ip[1]/ip[3] is low and ip[0]/ip[2] is high.
  ipv6_addr->set_address_high(make_net_int64(ip[1], ip[0]));
  ipv6_addr->set_address_low(make_net_int64(ip[3], ip[2]));
  ipv6_addr->set_port(port);

  return addr;
}

NetworkAddress* NetworkSignalFormatter::CreateUnixAddress(uint64_t id, const sinsp_fdinfo_t* fd_info) {
  if (!id) return nullptr;
  auto addr = Allocate<NetworkAddress>();
  auto local_addr = addr->mutable_local_address();
  local_addr->set_id(id);
  if (fd_info) {
    const std::string& fd_name = fd_info->m_name;
    // fd_info->m_name sometimes contains just the path, sometimes it contains a tuple of form "<id>-><id> <path>".
    auto space_pos = fd_name.rfind(' ');
    if (space_pos != std::string::npos) {
      local_addr->set_path(fd_name.substr(space_pos + 1));
    } else {
      local_addr->set_path(fd_name);
    }
  }
  return addr;
}

}  // namespace collector
