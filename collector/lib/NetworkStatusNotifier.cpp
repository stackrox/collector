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

#include "NetworkStatusNotifier.h"

#include <google/protobuf/util/time_util.h>

#include "CollectorStats.h"
#include "DuplexGRPC.h"
#include "GRPCUtil.h"
#include "Profiler.h"
#include "ProtoUtil.h"
#include "TimeUtil.h"
#include "Utility.h"

namespace collector {

namespace {

storage::L4Protocol TranslateL4Protocol(L4Proto proto) {
  switch (proto) {
    case L4Proto::TCP:
      return storage::L4_PROTOCOL_TCP;
    case L4Proto::UDP:
      return storage::L4_PROTOCOL_UDP;
    case L4Proto::ICMP:
      return storage::L4_PROTOCOL_ICMP;
    default:
      return storage::L4_PROTOCOL_UNKNOWN;
  }
}

sensor::SocketFamily TranslateAddressFamily(Address::Family family) {
  switch (family) {
    case Address::Family::IPV4:
      return sensor::SOCKET_FAMILY_IPV4;
    case Address::Family::IPV6:
      return sensor::SOCKET_FAMILY_IPV6;
    default:
      return sensor::SOCKET_FAMILY_UNKNOWN;
  }
}

}  // namespace

std::vector<IPNet> readNetworks(const string& networks, Address::Family family) {
  int tuple_size = Address::Length(family) + 1;
  int num_nets = networks.size() / tuple_size;
  std::vector<IPNet> ip_nets;
  ip_nets.reserve(num_nets);
  for (int i = 0; i < num_nets; i++) {
    // Bytes are received in big-endian order.
    std::array<uint64_t, Address::kU64MaxLen> ip = {};
    std::memcpy(&ip, &networks.c_str()[tuple_size * i], Address::Length(family));
    IPNet net(Address(family, ip), static_cast<int>(networks.c_str()[tuple_size * i + tuple_size - 1]));
    ip_nets.push_back(net);
  }
  return ip_nets;
}

void NetworkStatusNotifier::OnRecvControlMessage(const sensor::NetworkFlowsControlMessage* msg) {
  if (!msg) {
    return;
  }
  if (msg->has_ip_networks()) {
    ReceivePublicIPs(msg->public_ip_addresses());
  }
  if (msg->has_ip_networks()) {
    ReceiveIPNetworks(msg->ip_networks());
  }
}

void NetworkStatusNotifier::ReceivePublicIPs(const sensor::IPAddressList& public_ips) {
  UnorderedSet<Address> known_public_ips;
  for (const uint32_t public_ip : public_ips.ipv4_addresses()) {
    Address addr(htonl(public_ip));
    known_public_ips.insert(addr);
    known_public_ips.insert(addr.ToV6());
  }

  auto ipv6_size = public_ips.ipv6_addresses_size();
  if (ipv6_size % 2 != 0) {
    CLOG(WARNING) << "IPv6 address field has odd length " << ipv6_size << ". Ignoring IPv6 addresses...";
  } else {
    for (int i = 0; i < ipv6_size; i += 2) {
      known_public_ips.emplace(htonll(public_ips.ipv6_addresses(i)), htonll(public_ips.ipv6_addresses(i + 1)));
    }
  }

  conn_tracker_->UpdateKnownPublicIPs(std::move(known_public_ips));
}

void NetworkStatusNotifier::ReceiveIPNetworks(const sensor::IPNetworkList& networks) {
  UnorderedMap<Address::Family, std::vector<IPNet>> known_ip_networks;
  auto ipv4_networks_size = networks.ipv4_networks().size();
  if (ipv4_networks_size % 5 != 0) {
    CLOG(WARNING) << "IPv4 network field has incorrect length " << ipv4_networks_size << ". Ignoring IPv4 networks...";
  } else {
    std::vector<IPNet> ipv4_networks = readNetworks(networks.ipv4_networks(), Address::Family::IPV4);
    known_ip_networks[Address::Family::IPV4] = ipv4_networks;
  }

  auto ipv6_networks_size = networks.ipv6_networks().size();
  if (ipv6_networks_size % 17 != 0) {
    CLOG(WARNING) << "IPv6 network field has incorrect length " << ipv6_networks_size << ". Ignoring IPv6 networks...";
  } else {
    std::vector<IPNet> ipv6_networks = readNetworks(networks.ipv6_networks(), Address::Family::IPV6);
    known_ip_networks[Address::Family::IPV6] = ipv6_networks;
  }
  conn_tracker_->UpdateKnownIPNetworks(std::move(known_ip_networks));
}

void NetworkStatusNotifier::Run() {
  Profiler::RegisterCPUThread();
  auto next_attempt = std::chrono::system_clock::now();

  while (thread_.PauseUntil(next_attempt)) {
    comm_->ResetClientContext();

    if (!comm_->WaitForConnectionReady([this] { return thread_.should_stop(); })) {
      break;
    }

    auto client_writer = comm_->PushNetworkConnectionInfoOpenStream([this](const sensor::NetworkFlowsControlMessage* msg) { OnRecvControlMessage(msg); });

    if (enable_afterglow_) {
      RunSingleAfterglow(client_writer.get());
    } else {
      RunSingle(client_writer.get());
    }
    if (thread_.should_stop()) {
      return;
    }
    auto status = client_writer->Finish(std::chrono::seconds(5));
    if (status.ok()) {
      CLOG(ERROR) << "Error streaming network connection info: server hung up unexpectedly";
    } else {
      CLOG(ERROR) << "Error streaming network connection info: " << status.error_message();
    }
    next_attempt = std::chrono::system_clock::now() + std::chrono::seconds(10);
  }

  CLOG(INFO) << "Stopped network status notifier.";
}

void NetworkStatusNotifier::Start() {
  thread_.Start([this] { Run(); });
  CLOG(INFO) << "Started network status notifier.";
}

void NetworkStatusNotifier::Stop() {
  comm_->TryCancel();
  thread_.Stop();
}

void NetworkStatusNotifier::WaitUntilWriterStarted(IDuplexClientWriter<sensor::NetworkConnectionInfoMessage>* writer, int wait_time_seconds) {
  if (!writer->WaitUntilStarted(std::chrono::seconds(wait_time_seconds))) {
    CLOG(ERROR) << "Failed to establish network connection info stream.";
    return;
  }

  CLOG(INFO) << "Established network connection info stream.";
}

bool NetworkStatusNotifier::UpdateAllConnsAndEndpoints() {
  if (turn_off_scraping_) {
    return true;
  }

  int64_t ts = NowMicros();
  std::vector<Connection> all_conns;
  std::vector<ContainerEndpoint> all_listen_endpoints;
  WITH_TIMER(CollectorStats::net_scrape_read) {
    bool success = conn_scraper_->Scrape(&all_conns, scrape_listen_endpoints_ ? &all_listen_endpoints : nullptr);
    if (!success) {
      CLOG(ERROR) << "Failed to scrape connections and no pending connections to send";
      return false;
    }
  }
  WITH_TIMER(CollectorStats::net_scrape_update) {
    conn_tracker_->Update(all_conns, all_listen_endpoints, ts);
  }

  return true;
}

void NetworkStatusNotifier::RunSingle(IDuplexClientWriter<sensor::NetworkConnectionInfoMessage>* writer) {
  WaitUntilWriterStarted(writer, 10);

  ConnMap old_conn_state;
  ContainerEndpointMap old_cep_state;
  auto next_scrape = std::chrono::system_clock::now();

  while (writer->Sleep(next_scrape)) {
    next_scrape = std::chrono::system_clock::now() + std::chrono::seconds(scrape_interval_);

    if (!UpdateAllConnsAndEndpoints()) {
      continue;
    }

    const sensor::NetworkConnectionInfoMessage* msg;
    ConnMap new_conn_state;
    ContainerEndpointMap new_cep_state;
    WITH_TIMER(CollectorStats::net_fetch_state) {
      new_conn_state = conn_tracker_->FetchConnState(true, true);
      ConnectionTracker::ComputeDelta(new_conn_state, &old_conn_state);

      new_cep_state = conn_tracker_->FetchEndpointState(true, true);
      ConnectionTracker::ComputeDelta(new_cep_state, &old_cep_state);
    }

    WITH_TIMER(CollectorStats::net_create_message) {
      msg = CreateInfoMessage(old_conn_state, old_cep_state);
      old_conn_state = std::move(new_conn_state);
      old_cep_state = std::move(new_cep_state);
    }

    if (!msg) {
      continue;
    }

    WITH_TIMER(CollectorStats::net_write_message) {
      if (!writer->Write(*msg, next_scrape)) {
        CLOG(ERROR) << "Failed to write network connection info";
        return;
      }
    }
  }
}

void NetworkStatusNotifier::RunSingleAfterglow(IDuplexClientWriter<sensor::NetworkConnectionInfoMessage>* writer) {
  WaitUntilWriterStarted(writer, 10);

  ConnMap old_conn_state;
  ContainerEndpointMap old_cep_state;
  auto next_scrape = std::chrono::system_clock::now();
  int64_t time_at_last_scrape = NowMicros();

  while (writer->Sleep(next_scrape)) {
    next_scrape = std::chrono::system_clock::now() + std::chrono::seconds(scrape_interval_);

    if (!UpdateAllConnsAndEndpoints()) {
      continue;
    }

    int64_t time_micros = NowMicros();
    const sensor::NetworkConnectionInfoMessage* msg;
    ContainerEndpointMap new_cep_state, delta_cep;
    ConnMap new_conn_state, delta_conn;
    WITH_TIMER(CollectorStats::net_fetch_state) {
      new_conn_state = conn_tracker_->FetchConnState(true, true);
      ConnectionTracker::ComputeDeltaAfterglow(new_conn_state, old_conn_state, delta_conn, time_micros, time_at_last_scrape, afterglow_period_micros_);

      new_cep_state = conn_tracker_->FetchEndpointState(true, true);
      ConnectionTracker::ComputeDeltaAfterglow(new_cep_state, old_cep_state, delta_cep, time_micros, time_at_last_scrape, afterglow_period_micros_);
    }

    WITH_TIMER(CollectorStats::net_create_message) {
      // Report the deltas
      msg = CreateInfoMessage(delta_conn, delta_cep);
      // Add new connections to the old_state and remove inactive connections that are older than the afterglow period.
      ConnectionTracker::UpdateOldState(&old_conn_state, new_conn_state, time_micros, afterglow_period_micros_);
      ConnectionTracker::UpdateOldState(&old_cep_state, new_cep_state, time_micros, afterglow_period_micros_);
      time_at_last_scrape = time_micros;
    }

    if (!msg) {
      continue;
    }

    WITH_TIMER(CollectorStats::net_write_message) {
      if (!writer->Write(*msg, next_scrape)) {
        CLOG(ERROR) << "Failed to write network connection info";
        return;
      }
    }
  }
}

sensor::NetworkConnectionInfoMessage* NetworkStatusNotifier::CreateInfoMessage(const ConnMap& conn_delta, const ContainerEndpointMap& endpoint_delta) {
  if (conn_delta.empty() && endpoint_delta.empty()) return nullptr;

  Reset();
  auto* msg = AllocateRoot();
  auto* info = msg->mutable_info();

  AddConnections(info->mutable_updated_connections(), conn_delta);
  COUNTER_ADD(CollectorStats::net_conn_deltas, conn_delta.size());
  AddContainerEndpoints(info->mutable_updated_endpoints(), endpoint_delta);
  COUNTER_ADD(CollectorStats::net_cep_deltas, endpoint_delta.size());

  *info->mutable_time() = CurrentTimeProto();

  return msg;
}

void NetworkStatusNotifier::AddConnections(::google::protobuf::RepeatedPtrField<sensor::NetworkConnection>* updates, const ConnMap& delta) {
  for (const auto& delta_entry : delta) {
    auto* conn_proto = ConnToProto(delta_entry.first);
    if (!delta_entry.second.IsActive()) {
      *conn_proto->mutable_close_timestamp() = google::protobuf::util::TimeUtil::MicrosecondsToTimestamp(
          delta_entry.second.LastActiveTime());
    }
    updates->AddAllocated(conn_proto);
  }
}

void NetworkStatusNotifier::AddContainerEndpoints(::google::protobuf::RepeatedPtrField<sensor::NetworkEndpoint>* updates, const ContainerEndpointMap& delta) {
  for (const auto& delta_entry : delta) {
    auto* endpoint_proto = ContainerEndpointToProto(delta_entry.first);
    if (!delta_entry.second.IsActive()) {
      *endpoint_proto->mutable_close_timestamp() = google::protobuf::util::TimeUtil::MicrosecondsToTimestamp(
          delta_entry.second.LastActiveTime());
    }
    updates->AddAllocated(endpoint_proto);
  }
}

sensor::NetworkConnection* NetworkStatusNotifier::ConnToProto(const Connection& conn) {
  auto* conn_proto = Allocate<sensor::NetworkConnection>();
  conn_proto->set_container_id(conn.container());
  conn_proto->set_role(conn.is_server() ? sensor::ROLE_SERVER : sensor::ROLE_CLIENT);
  conn_proto->set_protocol(TranslateL4Protocol(conn.l4proto()));
  conn_proto->set_socket_family(TranslateAddressFamily(conn.local().address().family()));
  conn_proto->set_allocated_local_address(EndpointToProto(conn.local()));
  conn_proto->set_allocated_remote_address(EndpointToProto(conn.remote()));

  return conn_proto;
}

sensor::NetworkEndpoint* NetworkStatusNotifier::ContainerEndpointToProto(const ContainerEndpoint& cep) {
  auto* endpoint_proto = Allocate<sensor::NetworkEndpoint>();
  endpoint_proto->set_container_id(cep.container());
  endpoint_proto->set_protocol(TranslateL4Protocol(cep.l4proto()));
  endpoint_proto->set_socket_family(TranslateAddressFamily(cep.endpoint().address().family()));
  endpoint_proto->set_allocated_listen_address(EndpointToProto(cep.endpoint()));

  return endpoint_proto;
}

sensor::NetworkAddress* NetworkStatusNotifier::EndpointToProto(const collector::Endpoint& endpoint) {
  if (endpoint.IsNull()) {
    return nullptr;
  }

  // Note: We are sending the address data and network data as separate fields for
  // backward compatibility, although, network field can handle both.
  // Sensor tries to match address to known cluster entities. If that fails, it tries
  // to match the network to known external networks,

  auto* addr_proto = Allocate<sensor::NetworkAddress>();
  auto addr_length = endpoint.address().length();
  if (endpoint.network().IsAddress()) {
    addr_proto->set_address_data(endpoint.address().data(), addr_length);
  }
  if (endpoint.network().bits() > 0) {
    std::array<uint8_t, Address::kMaxLen + 1> buff;
    std::memcpy(buff.data(), endpoint.network().address().data(), addr_length);
    buff[addr_length] = endpoint.network().bits();
    addr_proto->set_ip_network(buff.data(), addr_length + 1);
  }
  addr_proto->set_port(endpoint.port());

  return addr_proto;
}

}  // namespace collector
