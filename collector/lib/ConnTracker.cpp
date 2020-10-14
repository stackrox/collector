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
static const IPNet canonical_external_ipv4_network(canonical_external_ipv4_addr, 32);
static const IPNet canonical_external_ipv6_network(canonical_external_ipv6_addr, 128);

}  // namespace

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

IPNet ConnectionTracker::NormalizeAddressNoLock(const Address& address) const {
  if (address.IsNull()) {
    return {};
  }
  if (CLOG_ENABLED(DEBUG)) {
    CLOG(DEBUG) << "remote address: " << address << '\n';
  }
  // Try to associate address to known cluster entities first, even if it is contained by a known network. If an IP
  // address is not public, we always assume that it could be that of a known cluster entity.
  if (!address.IsPublic() || Contains(known_public_ips_, address)) {
    return IPNet(address, 8 * address.length());
  }

  // If association to known cluster entities fails, then try to associate the address to a known network. Since the
  // networks are sorted highest-smallest to lowest-largest within family, we map the address to first matched subnet.
  // We do not want to map to all networks that contains this address, for example, if there is also a supernet in
  // known network list, this address would not be mapped to the supernet.
  const auto* networks = Lookup(known_ip_networks_, address.family());
  if (networks) {
    for (const auto& network : *networks) {
      if (network.Contains(address)) {
        return network;
      }
    }
  }

  // Otherwise, associate it to "rest of the internet".
  switch (address.family()) {
    case Address::Family::IPV4:
      return canonical_external_ipv4_network;
    case Address::Family::IPV6:
      return canonical_external_ipv6_network;
    default:
      return IPNet();
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
    local = Endpoint(Address(), conn.local().port());
    remote = Endpoint(NormalizeAddressNoLock(conn.remote().address()), 0);
  } else {
    // If this is the client, the local port and address are not relevant.
    local = Endpoint();
    remote = Endpoint(NormalizeAddressNoLock(remote.address()), remote.port());
  }

  return Connection(conn.container(), local, remote, conn.l4proto(), is_server);
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

template <typename T, typename ProcessFn>
UnorderedMap<T, ConnStatus> FetchState(UnorderedMap<T, ConnStatus>* state, bool clear_inactive, const ProcessFn& process_fn) {
  constexpr bool normalize = !std::is_same<ProcessFn, dont_normalize>::value;

  UnorderedMap<T, ConnStatus> fetched_state;

  if (!clear_inactive && !normalize) {
    return *state;
  }

  for (auto it = state->begin(); it != state->end(); ) {
    const auto& entry = *it;

    if (normalize) {
      auto emplace_res = fetched_state.emplace(process_fn(entry.first), entry.second);
      if (!emplace_res.second) {
        emplace_res.first->second.MergeFrom(entry.second);
      }
    } else {
      fetched_state.insert(entry);
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
    if (normalize) {
      return FetchState(&conn_state_, clear_inactive, [this](const Connection &conn) {
        return this->NormalizeConnectionNoLock(conn);
      });
    }

    return FetchState(&conn_state_, clear_inactive, dont_normalize());
  }
  return {};  // will never happen
}

ContainerEndpointMap ConnectionTracker::FetchEndpointState(bool normalize, bool clear_inactive) {
  WITH_LOCK(mutex_) {
    if (normalize) {
      return FetchState(&endpoint_state_, clear_inactive, [this](const ContainerEndpoint &cep) {
        const auto& ep = cep.endpoint();
        return ContainerEndpoint(cep.container(), Endpoint(Address(ep.address().family()), ep.port()), cep.l4proto());
      });
    }

    return FetchState(&endpoint_state_, clear_inactive, dont_normalize());
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
  WITH_LOCK(mutex_) {
    known_ip_networks_ = std::move(known_ip_networks);
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
}  // namespace collector
