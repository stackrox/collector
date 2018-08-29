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

#include "CollectorSignalFormatter.h"

#include <uuid/uuid.h>

#include "EventMap.h"
#include "Logging.h"

namespace collector {

using SignalStreamMessage = v1::SignalStreamMessage;
using Signal = CollectorSignalFormatter::Signal;
using ProcessSignal = CollectorSignalFormatter::ProcessSignal;
using ProcessCredentials = ProcessSignal::Credentials;

using NetworkSignal = CollectorSignalFormatter::NetworkSignal;
using NetworkAddress = CollectorSignalFormatter::NetworkAddress;
using L4Protocol = v1::L4Protocol;
using SocketFamily = v1::SocketFamily;
using IPV4NetworkAddress = CollectorSignalFormatter::IPV4NetworkAddress;
using IPV6NetworkAddress = CollectorSignalFormatter::IPV6NetworkAddress;

namespace {

uint64_t make_net_int64(uint32_t net_low, uint32_t net_high) {
  return static_cast<uint64_t>(ntohl(net_high)) << 32 | static_cast<uint64_t>(ntohl(net_low));
}

enum ProcessSignalType {
	EXECVE,
	UNKNOWN_PROCESS_TYPE
};

static EventMap<ProcessSignalType> process_signals = {
  {
    { "execve<", ProcessSignalType::EXECVE },
  },
  ProcessSignalType::UNKNOWN_PROCESS_TYPE,
};

enum NetworkSignalType {
	CONNECT,
	ACCEPT,
	UNKNOWN_NETWORK_TYPE
};

EventMap<NetworkSignalType> network_signals = {
  {
    { "connect<", NetworkSignalType::CONNECT },
    { "accept<", NetworkSignalType::ACCEPT },
  },
  NetworkSignalType::UNKNOWN_NETWORK_TYPE,
};

}

const SignalStreamMessage* CollectorSignalFormatter::ToProtoMessage(sinsp_evt* event) {
  if (!process_signals[event->get_type()] && !network_signals[event->get_type()]) {
    return nullptr;
  }

  Signal* signal;
  ProcessSignal* process_signal;
  NetworkSignal* network_signal;

  // set process info
  if (process_signals[event->get_type()]) {
    process_signal = CreateProcessSignal(event);
    if (!process_signal) return nullptr;
    signal = Allocate<Signal>();
    signal->set_allocated_process_signal(process_signal);
  }

  // set network info
  if (network_signals[event->get_type()]) {
    network_signal = CreateNetworkSignal(event);
    if (!network_signal) return nullptr;
    signal = Allocate<Signal>();
    signal->set_allocated_network_signal(network_signal);
  }

  // set id
  uuid_t uuid;
  constexpr int kUuidStringLength = 36;  // uuid_unparse manpage says so. Feeling slightly uneasy still ...
  char uuid_str[kUuidStringLength + 1];
  uuid_generate_time_safe(uuid);
  uuid_unparse_lower(uuid, uuid_str);
  std::string id(uuid_str);
  signal->set_id(id);


  // set time
  // Fix this to proto.timestamp
  //signal->set_time_nanos(event->get_ts());

  // set container_id
  if (const std::string* container_id = event_extractor_.get_container_id(event)) {
    if (container_id->length() <= 12) {
      signal->set_container_id(*container_id);
    } else {
      signal->set_container_id(container_id->substr(0, 12));
    }
  }

  SignalStreamMessage* signal_stream_message = AllocateRoot();
  signal_stream_message->clear_collector_register_request();
  signal_stream_message->set_allocated_signal(signal);
  return signal_stream_message;
}

ProcessSignal* CollectorSignalFormatter::CreateProcessSignal(sinsp_evt* event) {
  auto signal = Allocate<ProcessSignal>();

  // set name
  if (const char* name = event_extractor_.get_proc_name(event)) signal->set_name(name);
  // set command_line
  if (const char* command_line = event_extractor_.get_exeline(event)) signal->set_command_line(command_line);
  // set pid
  if (const int64_t* pid = event_extractor_.get_pid(event)) signal->set_pid(*pid);
  // set exec_file_path
  if (const std::string* exe = event_extractor_.get_exe(event)) signal->set_exec_file_path(*exe);
  // set creds
  ProcessCredentials* process_creds = CreateProcessCreds(event);
  signal->set_allocated_credentials(process_creds);

  return signal;
}

ProcessCredentials* CollectorSignalFormatter::CreateProcessCreds(sinsp_evt* event) {
  auto creds = Allocate<ProcessCredentials>();

  if (const uint32_t* uid = event_extractor_.get_uid(event)) creds->set_uid(*uid);
  if (const uint32_t* gid = event_extractor_.get_gid(event)) creds->set_gid(*gid);
  // fill in remaining
  return creds;
}

NetworkSignal* CollectorSignalFormatter::CreateNetworkSignal(sinsp_evt* event) {
  sinsp_fdinfo_t*  fd_info = event->get_fd_info();
  if (!fd_info) return nullptr;
  if (fd_info->is_role_none()) return nullptr;

  auto signal = Allocate<NetworkSignal>();

  if (!fd_info->is_role_none()) {
    signal->set_role(fd_info->is_role_server() ? NetworkSignal::ROLE_SERVER : NetworkSignal::ROLE_CLIENT);
  }

  L4Protocol l4proto = L4Protocol::L4_PROTOCOL_UNKNOWN;
  switch (fd_info->get_l4proto()) {
    case SCAP_L4_TCP:
      l4proto = L4Protocol::L4_PROTOCOL_TCP;
      break;
    case SCAP_L4_UDP:
      l4proto = L4Protocol::L4_PROTOCOL_UDP;
      break;
    case SCAP_L4_ICMP:
      l4proto = L4Protocol::L4_PROTOCOL_ICMP;
      break;
    case SCAP_L4_RAW:
      l4proto = L4Protocol::L4_PROTOCOL_RAW;
      break;
    default:
      break;
  }

  if (l4proto != L4Protocol::L4_PROTOCOL_UNKNOWN) {
    signal->set_protocol(l4proto);
  }

  switch (fd_info->m_type) {
    case SCAP_FD_IPV4_SOCK: {
      const auto& ipv4_fields = fd_info->m_sockinfo.m_ipv4info.m_fields;
      signal->set_socket_family(SocketFamily::SOCKET_FAMILY_IPV4);
      signal->set_allocated_client_address(CreateIPv4Address(ipv4_fields.m_sip, ipv4_fields.m_sport));
      signal->set_allocated_server_address(CreateIPv4Address(ipv4_fields.m_dip, ipv4_fields.m_dport));
      break;
    }
    case SCAP_FD_IPV4_SERVSOCK: {
      const auto& ipv4_server_info = fd_info->m_sockinfo.m_ipv4serverinfo;
      signal->set_socket_family(SocketFamily::SOCKET_FAMILY_IPV4);
      signal->set_role(NetworkSignal::ROLE_SERVER);
      signal->set_allocated_server_address(CreateIPv4Address(ipv4_server_info.m_ip, ipv4_server_info.m_port));
      break;
    }
    case SCAP_FD_IPV6_SOCK: {
      const auto& ipv6_fields = fd_info->m_sockinfo.m_ipv6info.m_fields;
      signal->set_socket_family(SocketFamily::SOCKET_FAMILY_IPV6);
      signal->set_allocated_client_address(CreateIPv6Address(ipv6_fields.m_sip, ipv6_fields.m_sport));
      signal->set_allocated_server_address(CreateIPv6Address(ipv6_fields.m_dip, ipv6_fields.m_dport));
      break;
    }
    case SCAP_FD_IPV6_SERVSOCK: {
      const auto& ipv6_server_info = fd_info->m_sockinfo.m_ipv6serverinfo;
      signal->set_socket_family(SocketFamily::SOCKET_FAMILY_IPV6);
      signal->set_role(NetworkSignal::ROLE_SERVER);
      signal->set_allocated_server_address(CreateIPv6Address(ipv6_server_info.m_ip, ipv6_server_info.m_port));
      break;
    }
    default:
      break;
  }

  return signal;
}

NetworkAddress* CollectorSignalFormatter::CreateIPv4Address(uint32_t ip, uint16_t port) {
  if (!ip) return nullptr;
  auto addr = Allocate<NetworkAddress>();
  IPV4NetworkAddress* ipv4_addr = addr->mutable_ipv4_address();
  ipv4_addr->set_address(ntohl(ip));
  ipv4_addr->set_port(port);
  return addr;
}

NetworkAddress* CollectorSignalFormatter::CreateIPv6Address(const uint32_t (&ip)[4], uint16_t port) {
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

}  // namespace collector
