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

#include "Containers.h"
#include "DuplexGRPC.h"
#include "GRPCUtil.h"
#include "ProtoUtil.h"
#include "TimeUtil.h"
#include "Utility.h"

#include <google/protobuf/util/time_util.h>

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

constexpr char NetworkStatusNotifier::kHostnameMetadataKey[];
constexpr char NetworkStatusNotifier::kCapsMetadataKey[];
constexpr char NetworkStatusNotifier::kSupportedCaps[];

std::unique_ptr<grpc::ClientContext> NetworkStatusNotifier::CreateClientContext() const {
  auto ctx = MakeUnique<grpc::ClientContext>();
  ctx->AddMetadata(kHostnameMetadataKey, hostname_);
  ctx->AddMetadata(kCapsMetadataKey, kSupportedCaps);
  return ctx;
}

void NetworkStatusNotifier::OnRecvControlMessage(const sensor::NetworkFlowsControlMessage* msg) {
  if (!msg || !msg->has_public_ip_addresses()) {
    return;
  }

  const auto& public_ips = msg->public_ip_addresses();

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

  if (!msg->has_ip_networks()) {
    return;
  }

  ReceiveIPNetworks(msg->ip_networks());
}

void NetworkStatusNotifier::ReceiveIPNetworks(const sensor::IPNetworkList networks) {
  NetworkDescComparator compare;
  UnorderedMap<Address::Family, std::vector<IPNet>> known_ip_networks;
  auto ipv4_networks_size = networks.ipv4_networks().size();
  if (ipv4_networks_size % 5 != 0) {
    CLOG(WARNING) << "IPv4 network field has incorrect length " << ipv4_networks_size << ". Ignoring IPv4 networks...";
  } else {
    std::vector<IPNet> ipv4_networks = {};
    for (int i = 0; i < ipv4_networks_size; i += 5) {
      // Bytes are received in big-endian order.
      IPNet net(Address(networks.ipv4_networks().at(i),
                        networks.ipv4_networks().at(i + 1),
                        networks.ipv4_networks().at(i + 2),
                        networks.ipv4_networks().at(i + 3)
                        ),
                networks.ipv4_networks().at(i + 4));
      ipv4_networks.push_back(net);
    }

    // NetworkDescComparator is a comparator to sort the networks as highest-smallest to lowest-largest within the same family.
    std::sort(ipv4_networks.begin(), ipv4_networks.end(), compare);
    known_ip_networks[Address::Family::IPV4] = ipv4_networks;
  }

  auto ipv6_networks_size = networks.ipv6_networks().size();
  if (ipv6_networks_size % 17 != 0) {
    CLOG(WARNING) << "IPv6 network field has incorrect length " << ipv6_networks_size << ". Ignoring IPv6 networks...";
  } else {
    std::vector<IPNet> ipv6_networks = {};
    for (int i = 0; i < ipv6_networks_size; i += 17) {
      uint64_t high, low;
      // Bytes are received in big-endian order.
      for (int j = 0; j < 8; j++) {
        high |= static_cast<uint64_t>(networks.ipv6_networks().at(i + j)) << (8 * (7 - j));
      }

      // Bytes are received in big-endian order.
      for (int j = 8; j < 16; j++) {
        low |= static_cast<uint64_t>(networks.ipv6_networks().at(i + j)) << (8 * (15 - j));
      }
      Address addr(high, low);
      IPNet net(addr, networks.ipv6_networks().at(i + 16));
      ipv6_networks.push_back(net);
    }

    // NetworkDescComparator is a comparator to sort the networks as highest-smallest to lowest-largest within the same family.
    std::sort(ipv6_networks.begin(), ipv6_networks.end(), compare);
    known_ip_networks[Address::Family::IPV6] = ipv6_networks;
  }

  conn_tracker_->UpdateKnownIPNetworks(std::move(known_ip_networks));
}

void NetworkStatusNotifier::Run() {
  auto next_attempt = std::chrono::system_clock::now();

  while (thread_.PauseUntil(next_attempt)) {
    WITH_LOCK(context_mutex_) {
      context_ = CreateClientContext();
    }

    if (!WaitForChannelReady(channel_, [this] { return thread_.should_stop(); })) {
      break;
    }

    std::function<void(const sensor::NetworkFlowsControlMessage*)> read_cb = [this](const sensor::NetworkFlowsControlMessage* msg) {
      OnRecvControlMessage(msg);
    };

    auto client_writer = DuplexClient::CreateWithReadCallback(
        &sensor::NetworkConnectionInfoService::Stub::AsyncPushNetworkConnectionInfo,
        channel_, context_.get(), std::move(read_cb));

    RunSingle(client_writer.get());
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
  thread_.Start([this]{ Run(); });
  CLOG(INFO) << "Started network status notifier.";
}

void NetworkStatusNotifier::Stop() {
  WITH_LOCK(context_mutex_) {
    if (context_) context_->TryCancel();
  }
  thread_.Stop();
}

void NetworkStatusNotifier::RunSingle(DuplexClientWriter<sensor::NetworkConnectionInfoMessage>* writer) {
  if (!writer->WaitUntilStarted(std::chrono::seconds(10))) {
    CLOG(ERROR) << "Failed to establish network connection info stream.";
    return;
  }

  CLOG(INFO) << "Established network connection info stream.";

  ConnMap old_conn_state;
  ContainerEndpointMap old_cep_state;
  auto next_scrape = std::chrono::system_clock::now();

  while (writer->Sleep(next_scrape)) {
    next_scrape = std::chrono::system_clock::now() + std::chrono::seconds(scrape_interval_);

    if (!turn_off_scraping_) {
      int64_t ts = NowMicros();
      std::vector<Connection> all_conns;
      std::vector<ContainerEndpoint> all_listen_endpoints;
      bool success = conn_scraper_.Scrape(&all_conns, scrape_listen_endpoints_ ? &all_listen_endpoints : nullptr);

      if (!success) {
        CLOG(ERROR) << "Failed to scrape connections and no pending connections to send";
        continue;
      }
      conn_tracker_->Update(all_conns, all_listen_endpoints, ts);
    }

    auto new_conn_state = conn_tracker_->FetchConnState(true, true);
    ConnectionTracker::ComputeDelta(new_conn_state, &old_conn_state);

    auto new_cep_state = conn_tracker_->FetchEndpointState(true, true);
    ConnectionTracker::ComputeDelta(new_cep_state, &old_cep_state);

    const auto* msg = CreateInfoMessage(old_conn_state, old_cep_state);
    old_conn_state = std::move(new_conn_state);
    old_cep_state = std::move(new_cep_state);

    if (!msg) {
      continue;
    }

    if (!writer->Write(*msg, next_scrape)) {
      CLOG(ERROR) << "Failed to write network connection info";
      return;
    }
  }
}

sensor::NetworkConnectionInfoMessage* NetworkStatusNotifier::CreateInfoMessage(const ConnMap& conn_delta, const ContainerEndpointMap& endpoint_delta) {
  if (conn_delta.empty() && endpoint_delta.empty()) return nullptr;

  Reset();
  auto* msg = AllocateRoot();
  auto* info = msg->mutable_info();

  AddConnections(info->mutable_updated_connections(), conn_delta);
  AddContainerEndpoints(info->mutable_updated_endpoints(), endpoint_delta);

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

  auto* addr_proto = Allocate<sensor::NetworkAddress>();
  if (!endpoint.address().IsNull()) {
    addr_proto->set_address_data(endpoint.address().data(), endpoint.address().length());
  } else if (!endpoint.network().IsNull()) {
    std::array<uint8_t, 17> network_data = {};
    memcpy(&network_data, endpoint.address().data(), endpoint.network().address().length());
    network_data[endpoint.network().address().length() - 1] = endpoint.network().bits();
    addr_proto->set_ip_network(network_data.data(), endpoint.network().address().length() + sizeof(endpoint.network().bits()));
  }
  addr_proto->set_port(endpoint.port());

  return addr_proto;
}

}  // namespace collector
