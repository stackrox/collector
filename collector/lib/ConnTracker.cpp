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

#include "ConnTracker.h"

#include <utility>

#include "Containers.h"
#include "Logging.h"
#include "Utility.h"

namespace collector {

namespace {

static const Address canonical_external_ipv4_addr(255, 255, 255, 255);
static const Address canonical_external_ipv6_addr(0xffffffffffffffffULL, 0xffffffffffffffffULL);

}  // namespace

bool ContainsPrivateNetwork(Address::Family family, const std::vector<IPNet>& networks) {
  for (const auto& net : networks) {
    // Check if user-defined network is contained in private IP space or vice-versa.
    for (const auto& pNet : PrivateNetworks(family)) {
      if (pNet.Contains(net.address()) || net.Contains(pNet.address())) {
        return true;
      }
    }
  }
  return false;
}

void ConnectionTracker::UpdateConnection(const Connection& conn, int64_t timestamp, bool added) {
  WITH_LOCK(mutex_) {
    EmplaceOrUpdateNoLock(conn, ConnStatus(timestamp, added));
  }
}

void ConnectionTracker::Update(
        const std::vector<Connection>& all_conns,
        const std::vector<ContainerEndpoint>& all_listen_endpoints,
        int64_t timestamp) {
  WITH_LOCK(mutex_) {
    // Mark all existing connections and listen endpoints as inactive
    for (auto& prev_conn : conn_state_) {
      prev_conn.second.SetActive(false);
    }
    for (auto& prev_endpoint : endpoint_state_) {
      prev_endpoint.second.SetActive(false);
    }

    ConnStatus new_status(timestamp, true);

    // Insert (or mark as active) all current connections and listen endpoints.
    for (const auto& curr_conn : all_conns) {
      EmplaceOrUpdateNoLock(curr_conn, new_status);
    }
    for (const auto& curr_endpoint : all_listen_endpoints) {
      EmplaceOrUpdateNoLock(curr_endpoint, new_status);
    }
  }
}

IPNet ConnectionTracker::DetermineNetworkNoLock(const Address& address) const {
  // Since the networks are sorted highest-smallest to lowest-largest within family, we map the address to first
  // matched subnet.
  //
  // We do not want to map to all networks that contains this address, for example, if there is also a supernet in
  // known network list, this address would not be mapped to the supernet.
  const auto* networks = Lookup(known_ip_networks_, address.family());
  if (!networks) {
    return {};
  }

  for (const auto &network : *networks) {
    if (network.Contains(address)) {
      return network;
    }
  }
  return {};
}

IPNet ConnectionTracker::NormalizeAddressNoLock(const Address& address) const {
  if (address.IsNull()) {
    return {};
  }

  bool private_addr = !address.IsPublic();
  if (private_addr && !Lookup(known_private_networks_exists_, address.family()))  {
    return IPNet(address, 0, true);
  }

  const auto& network = DetermineNetworkNoLock(address);
  if (private_addr || Contains(known_public_ips_, address)) {
    return IPNet(address, network.bits(), true);
  }

  if (!network.IsNull()) {
    return network;
  }

  // Otherwise, associate it to "rest of the internet".
  switch (address.family()) {
    case Address::Family::IPV4:
      return IPNet(canonical_external_ipv4_addr, 0, true);
    case Address::Family::IPV6:
      return IPNet(canonical_external_ipv6_addr, 0, true);
    default:
      return {};
  }
}

Connection ConnectionTracker::NormalizeConnectionNoLock(const Connection& conn) const {
  bool is_server = conn.is_server();
  if (conn.l4proto() == L4Proto::UDP) {
    // Inference of server role is unreliable for UDP, so go by port.
    is_server = IsEphemeralPort(conn.remote().port()) > IsEphemeralPort(conn.local().port());
  }

  Endpoint local, remote = conn.remote();

  if (is_server) {
    // If this is the server, only the local port is relevant, while the remote port does not matter.
    local = Endpoint(IPNet(Address()), conn.local().port());
    remote = Endpoint(NormalizeAddressNoLock(conn.remote().address()), 0);
  } else {
    // If this is the client, the local port and address are not relevant.
    local = Endpoint();
    remote = Endpoint(NormalizeAddressNoLock(remote.address()), remote.port());
  }

  return Connection(conn.container(), local, remote, conn.l4proto(), is_server);
}

ContainerEndpoint ConnectionTracker::NormalizeContainerEndpoint(const ContainerEndpoint &cep) const {
  const auto& ep = cep.endpoint();
  return ContainerEndpoint(cep.container(), Endpoint(Address(ep.address().family()), ep.port()), cep.l4proto());
}

bool ConnectionTracker::ShouldFetchConnection(const Connection &conn) const {
  return !IsIgnoredL4ProtoPortPair(L4ProtoPortPair(conn.l4proto(), conn.local().port())) &&
         !IsIgnoredL4ProtoPortPair(L4ProtoPortPair(conn.l4proto(), conn.remote().port()));
}

bool ConnectionTracker::ShouldFetchContainerEndpoint(const ContainerEndpoint &cep) const {
  return !IsIgnoredL4ProtoPortPair(L4ProtoPortPair(cep.l4proto(), cep.endpoint().port()));
}

bool ConnectionTracker::IsIgnoredL4ProtoPortPair(const L4ProtoPortPair &p) const {
  return Contains(ignored_l4proto_port_pairs_, p);
}

namespace {

template <typename T>
void EmplaceOrUpdate(UnorderedMap<T, ConnStatus>* m, const T& obj, ConnStatus status) {
  auto emplace_res = m->emplace(obj, status);
  if (!emplace_res.second && status.LastActiveTime() > emplace_res.first->second.LastActiveTime()) {
    emplace_res.first->second = status;
  }
}

}  // namespace

void ConnectionTracker::EmplaceOrUpdateNoLock(const Connection& conn, ConnStatus status) {
  EmplaceOrUpdate(&conn_state_, conn, status);
}

void ConnectionTracker::EmplaceOrUpdateNoLock(const ContainerEndpoint& ep, ConnStatus status) {
  EmplaceOrUpdate(&endpoint_state_, ep, status);
}

namespace {

struct dont_normalize {
  template<typename T>
  inline auto operator()(T&& arg) const -> decltype(std::forward<T>(arg)) {
    return std::forward<T>(arg);
  }
};

struct dont_filter {
  template<typename T>
  inline auto operator()(T&& arg) const -> bool {
    return true;
  }
};

template <typename T, typename ProcessFn, typename FilterFn>
UnorderedMap<T, ConnStatus> FetchState(UnorderedMap<T, ConnStatus>* state, bool clear_inactive,
                                       const ProcessFn& process_fn, const FilterFn& filter_fn) {
  constexpr bool normalize = !std::is_same<ProcessFn, dont_normalize>::value;
  constexpr bool filter = !std::is_same<FilterFn, dont_filter>::value;

  UnorderedMap<T, ConnStatus> fetched_state;

  if (!clear_inactive && !normalize) {
    return *state;
  }

  for (auto it = state->begin(); it != state->end(); ) {
    const auto& entry = *it;

    if (!filter || filter_fn(entry.first)) {
      if (normalize) {
        auto emplace_res = fetched_state.emplace(process_fn(entry.first), entry.second);
        if (!emplace_res.second) {
          emplace_res.first->second.MergeFrom(entry.second);
        }
      } else {
        fetched_state.insert(entry);
      }
    }

    if (clear_inactive && !entry.second.IsActive()) {
      it = state->erase(it);
    } else {
      ++it;
    }
  }

  return fetched_state;
}

}  // namespace

ConnMap ConnectionTracker::FetchConnState(bool normalize, bool clear_inactive) {
  WITH_LOCK(mutex_) {
    std::function<bool(const Connection&)> filter_fn = dont_filter();
    std::function<const Connection(const Connection&)> process_fn = dont_normalize();
    if (IsConnectionFilteringEnabled()) {
      filter_fn = [this](const Connection &conn) {
        return this->ShouldFetchConnection(conn);
      };
    }
    if (normalize) {
      process_fn = [this](const Connection &conn) {
        return this->NormalizeConnectionNoLock(conn);
      };
    }
    return FetchState(&conn_state_, clear_inactive, process_fn, filter_fn);
  }
  return {};  // will never happen
}

ContainerEndpointMap ConnectionTracker::FetchEndpointState(bool normalize, bool clear_inactive) {
  WITH_LOCK(mutex_) {
    std::function<bool(const ContainerEndpoint&)> filter_fn = dont_filter();
    std::function<const ContainerEndpoint(const ContainerEndpoint&)> process_fn = dont_normalize();
    if (IsConnectionFilteringEnabled()) {
      filter_fn = [this] (const ContainerEndpoint &cep) {
        return this->ShouldFetchContainerEndpoint(cep);
      };
    }
    if (normalize) {
      process_fn = [this](const ContainerEndpoint &cep) {
        return this->NormalizeContainerEndpoint(cep);
      };
    }
    return FetchState(&endpoint_state_, clear_inactive, process_fn, filter_fn);
  }
  return {};  // will never happen
}

void ConnectionTracker::UpdateKnownPublicIPs(collector::UnorderedSet<collector::Address>&& known_public_ips) {
  WITH_LOCK(mutex_) {
    known_public_ips_ = std::move(known_public_ips);
    if (CLOG_ENABLED(DEBUG)) {
      CLOG(DEBUG) << "known public ips:";
      for (const auto &public_ip : known_public_ips_) {
        CLOG(DEBUG) << " - " << public_ip;
      }
    }
  }
}

void ConnectionTracker::UpdateKnownIPNetworks(UnorderedMap<Address::Family, std::vector<IPNet>>&& known_ip_networks) {
  UnorderedMap<Address::Family, bool> known_private_networks_exists;
  for (const auto& network_pair : known_ip_networks) {
    known_private_networks_exists[network_pair.first] = ContainsPrivateNetwork(network_pair.first, network_pair.second);
  }

  WITH_LOCK(mutex_) {
    known_ip_networks_ = std::move(known_ip_networks);
    known_private_networks_exists_ = std::move(known_private_networks_exists);
    if (CLOG_ENABLED(DEBUG)) {
      CLOG(DEBUG) << "known ip networks:";
      for (const auto &network_pair : known_ip_networks_) {
        for (const auto network : network_pair.second) {
          CLOG(DEBUG) << " - " << network;
        }
      }
    }
  }
}

void ConnectionTracker::UpdateIgnoredL4ProtoPortPairs(UnorderedSet<L4ProtoPortPair> &&ignored_l4proto_port_pairs) {
  WITH_LOCK(mutex_) {
    ignored_l4proto_port_pairs_ = std::move(ignored_l4proto_port_pairs);
    if (CLOG_ENABLED(DEBUG)) {
      CLOG(DEBUG) << "ignored l4 protocol and port pairs";
      for (const auto &proto_port_pair : ignored_l4proto_port_pairs_) {
        CLOG(DEBUG) << proto_port_pair.first << "/" << proto_port_pair.second;
      }
    }
  }
}
}  // namespace collector
